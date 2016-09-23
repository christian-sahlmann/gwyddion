/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <app/gwyapp.h>

#include "gwyddion.h"
#include "toolbox.h"

#define TOOLBOX_UI_FILE_NAME "toolbox.xml"

static const GwyEnum mode_types_array[] = {
    { "default",         0,                   },
    { "interactive",     GWY_RUN_INTERACTIVE, },
    { "non-interactive", GWY_RUN_IMMEDIATE,   },
    { NULL,              0,                   },
};

static const GwyEnum action_types_array[] = {
    { "empty",   GWY_APP_ACTION_TYPE_PLACEHOLDER, },
    { "builtin", GWY_APP_ACTION_TYPE_BUILTIN,     },
    { "proc",    GWY_APP_ACTION_TYPE_PROC,        },
    { "graph",   GWY_APP_ACTION_TYPE_GRAPH,       },
    { "volume",  GWY_APP_ACTION_TYPE_VOLUME,      },
    { "xyz",     GWY_APP_ACTION_TYPE_XYZ,         },
    { "tool",    GWY_APP_ACTION_TYPE_TOOL,        },
    { NULL,      0,                               },
};

const GwyEnum *gwy_toolbox_mode_types = mode_types_array;
const GwyEnum *gwy_toolbox_action_types = action_types_array;

static void
toolbox_ui_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                         const gchar *name,
                         const gchar **attribute_names,
                         const gchar **attribute_values,
                         gpointer user_data,
                         G_GNUC_UNUSED GError **error)
{
    GwyToolboxSpec *spec = (GwyToolboxSpec*)user_data;
    const gchar *attname, *attval;
    GString *path = spec->path;
    guint i, l;

    g_string_append_c(spec->path, '/');
    g_string_append(spec->path, name);

    if (gwy_strequal(name, "toolbox")) {
        gint vi;

        if (!gwy_strequal(path->str, "/toolbox")) {
            g_warning("Ignoring non top-level <toolbox>.");
            return;
        }

        for (i = 0; (attname = attribute_names[i]); i++) {
            attval = attribute_values[i];
            if (gwy_strequal(attname, "width")) {
                vi = atoi(attval);
                if (vi >= 0 && vi < 1024)
                    spec->width = vi;
                else
                    g_warning("Ignoring wrong toolbox width %d.", vi);
            }
            else {
                gwy_debug("Unimplemented <toolbox> attribute %s.", attname);
            }
        }
    }
    else if (gwy_strequal(name, "group")) {
        GArray *group = spec->group;
        const gchar *id = NULL, *title = NULL;
        gboolean translatable = FALSE;
        GwyToolboxGroupSpec *othergspec;
        GwyToolboxGroupSpec gspec;

        if (!gwy_strequal(path->str, "/toolbox/group")) {
            g_warning("Ignoring <group> not in <toolbox>.");
            return;
        }

        for (i = 0; (attname = attribute_names[i]); i++) {
            attval = attribute_values[i];
            if (gwy_strequal(attname, "id")) {
                if (gwy_strisident(attval, NULL, NULL))
                    id = attval;
                else
                    g_warning("Ignoring non-identifier id=\"%s\".", attval);
            }
            else if (gwy_strequal(attname, "title")) {
                if ((l = strlen(attval))
                    && g_utf8_validate(attval, l, NULL))
                    title = attval;
                else
                    g_warning("Ignoring invalid group title.");
            }
            else if (gwy_strequal(attname, "translatable")) {
                if (g_ascii_strcasecmp(attval, "true") == 0
                    || g_ascii_strcasecmp(attval, "yes") == 0)
                    translatable = TRUE;
                else if (g_ascii_strcasecmp(attval, "false") == 0
                    || g_ascii_strcasecmp(attval, "no") == 0)
                    translatable = FALSE;
                else
                    g_warning("Ignoring invalid group translatable attribute.");
            }
        }

        if (!id || !title) {
            g_warning("Ignoring <group> with missing/invalid id or title.");
            return;
        }

        for (i = 0; i < group->len; i++) {
            othergspec = &g_array_index(group, GwyToolboxGroupSpec, i);
            if (gwy_strequal(id, g_quark_to_string(othergspec->id))) {
                g_warning("Ignoring <group> with duplicate id \"%s\".", id);
                return;
            }
        }

        gspec.id = g_quark_from_string(id);
        gspec.name = g_strdup(title);
        gspec.translatable = translatable;
        gspec.item = g_array_new(FALSE, FALSE, sizeof(GwyToolboxItemSpec));
        g_array_append_val(group, gspec);
    }
    else if (gwy_strequal(name, "item")) {
        GArray *group = spec->group;
        GwyToolboxGroupSpec *gspec;
        const gchar *function = NULL, *icon = NULL;
        GwyAppActionType tmptype, type = GWY_APP_ACTION_TYPE_NONE;
        GwyRunType tmpmode, mode = 0;
        GwyToolboxItemSpec ispec;

        if (!gwy_strequal(path->str, "/toolbox/group/item") || !group->len) {
            g_warning("Ignoring <item> not in a <group>");
            return;
        }

        gspec = &g_array_index(group, GwyToolboxGroupSpec, group->len-1);

        for (i = 0; (attname = attribute_names[i]); i++) {
            attval = attribute_values[i];
            if (gwy_strequal(attname, "type")
                && (tmptype = gwy_string_to_enum(attval,
                                                 gwy_toolbox_action_types, -1))
                != -1)
                type = tmptype;
            else if (gwy_strequal(attname, "run")
                     && (tmpmode = gwy_string_to_enum(attval,
                                                      gwy_toolbox_mode_types,
                                                      -1))
                != -1)
                mode = tmpmode;
            else if (gwy_strequal(attname, "function"))
                function = attval;
            else if (gwy_strequal(attname, "icon"))
                icon = attval;
        }

        if (type == GWY_APP_ACTION_TYPE_NONE) {
            g_warning("Ignoring <item> without function type.");
            return;
        }
        if (!function && type != GWY_APP_ACTION_TYPE_TOOL) {
            g_warning("Ignoring <item> without function name.");
            return;
        }
        if (!function && type == GWY_APP_ACTION_TYPE_TOOL) {
            if (spec->seen_tool_placeholder) {
                g_warning("Ignoring duplicate tool placeholder <item>.");
                return;
            }
            spec->seen_tool_placeholder = TRUE;
        }
        if (type == GWY_APP_ACTION_TYPE_TOOL
            && function
            && !gwy_strisident(function, "_-", NULL)) {
            g_warning("Ignoring tool item with invalid function=\"%s\"",
                      function);
            return;
        }
        if (type == GWY_APP_ACTION_TYPE_PLACEHOLDER
            && (function || icon || mode)) {
            g_warning("Placeholder <item> should not have any attributes.");
            function = icon = NULL;
            mode = 0;
        }

        /* Icon and mode can be left unspecified. */
        ispec.type = type;
        ispec.function = function ? g_quark_from_string(function) : 0;
        ispec.icon = icon ? g_quark_from_string(icon) : 0;
        ispec.mode = mode;
        g_array_append_val(gspec->item, ispec);
    }
}

static void
toolbox_ui_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                       const gchar *name,
                       gpointer user_data,
                       G_GNUC_UNUSED GError **error)
{
    GwyToolboxSpec *spec = (GwyToolboxSpec*)user_data;
    gchar *p;

    p = strrchr(spec->path->str, '/');
    g_return_if_fail(p);
    g_return_if_fail(gwy_strequal(p + 1, name));
    g_string_truncate(spec->path, p - spec->path->str);
}

static void
toolbox_ui_text(G_GNUC_UNUSED GMarkupParseContext *context,
                const gchar *text,
                gsize text_len,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED GError **error)
{
    /* GwyToolboxSpec *spec = (GwyToolboxSpec*)user_data; */
    gsize i;

    for (i = 0; i < text_len; i++) {
        if (!g_ascii_isspace(text[i])) {
            g_warning("Non element content: %s", text);
            return;
        }
    }
}

void
gwy_toolbox_spec_free(GwyToolboxSpec *spec)
{
    GArray *group = spec->group;
    guint i;

    if (group) {
        for (i = 0; i < group->len; i++) {
            GwyToolboxGroupSpec *gspec = &g_array_index(group,
                                                        GwyToolboxGroupSpec, i);

            g_free(gspec->name);
            if (gspec->item)
                g_array_free(gspec->item, TRUE);
        }

        g_array_free(group, TRUE);
    }

    if (spec->path)
        g_string_free(spec->path, TRUE);

    g_free(spec);
}

GwyToolboxSpec*
gwy_toolbox_parse(const gchar *ui, gsize ui_len, GError **error)
{
    static const GMarkupParser parser = {
        toolbox_ui_start_element,
        toolbox_ui_end_element,
        toolbox_ui_text,
        NULL,
        NULL
    };

    GwyToolboxSpec *spec;
    GMarkupParseContext *context;

    spec = g_new0(GwyToolboxSpec, 1);
    spec->path = g_string_new(NULL);
    spec->group = g_array_new(FALSE, FALSE, sizeof(GwyToolboxGroupSpec));

    context = g_markup_parse_context_new(&parser, 0, spec, NULL);
    if (!g_markup_parse_context_parse(context, ui, ui_len, error)) {
        gwy_toolbox_spec_free(spec);
        spec = NULL;
    }
    g_markup_parse_context_free(context);

    return spec;
}

void
gwy_toolbox_spec_remove_item(GwyToolboxSpec *spec, guint i, guint j)
{
    GwyToolboxGroupSpec *gspec;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);
    g_return_if_fail(j < gspec->item->len);
    g_array_remove_index(gspec->item, j);
}

void
gwy_toolbox_spec_remove_group(GwyToolboxSpec *spec, guint i)
{
    GwyToolboxGroupSpec *gspec;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);
    g_free(gspec->name);
    if (gspec->item)
        g_array_free(gspec->item, TRUE);
    g_array_remove_index(spec->group, i);
}

void
gwy_toolbox_spec_move_item(GwyToolboxSpec *spec,
                           guint i,
                           guint j,
                           gboolean up)
{
    GwyToolboxGroupSpec *gspec;
    GwyToolboxItemSpec ispec;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);
    g_return_if_fail(j < gspec->item->len);
    ispec = g_array_index(gspec->item, GwyToolboxItemSpec, j);
    if (up) {
        g_return_if_fail(j > 0);
        g_array_index(gspec->item, GwyToolboxItemSpec, j)
            = g_array_index(gspec->item, GwyToolboxItemSpec, j-1);
        g_array_index(gspec->item, GwyToolboxItemSpec, j-1) = ispec;
    }
    else {
        g_return_if_fail(j+1 < gspec->item->len);
        g_array_index(gspec->item, GwyToolboxItemSpec, j)
            = g_array_index(gspec->item, GwyToolboxItemSpec, j+1);
        g_array_index(gspec->item, GwyToolboxItemSpec, j+1) = ispec;
    }
}

void
gwy_toolbox_spec_move_group(GwyToolboxSpec *spec,
                            guint i,
                            gboolean up)
{
    GwyToolboxGroupSpec gspec;

    g_return_if_fail(i < spec->group->len);
    gspec = g_array_index(spec->group, GwyToolboxGroupSpec, i);
    if (up) {
        g_return_if_fail(i > 0);
        g_array_index(spec->group, GwyToolboxGroupSpec, i)
            = g_array_index(spec->group, GwyToolboxGroupSpec, i-1);
        g_array_index(spec->group, GwyToolboxGroupSpec, i-1) = gspec;
    }
    else {
        g_return_if_fail(i+1 < spec->group->len);
        g_array_index(spec->group, GwyToolboxGroupSpec, i)
            = g_array_index(spec->group, GwyToolboxGroupSpec, i+1);
        g_array_index(spec->group, GwyToolboxGroupSpec, i+1) = gspec;
    }
}

void
gwy_toolbox_spec_add_item(GwyToolboxSpec *spec,
                          GwyToolboxItemSpec *ispec,
                          guint i,
                          guint j)
{
    GwyToolboxGroupSpec *gspec;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);

    if (j >= gspec->item->len)
        g_array_append_vals(gspec->item, ispec, 1);
    else
        g_array_insert_vals(gspec->item, j, ispec, 1);
}

/* This consumes any dynamically allocated data in @gspec.  The caller must
 * not free them! */
void
gwy_toolbox_spec_add_group(GwyToolboxSpec *spec,
                           GwyToolboxGroupSpec *gspec,
                           guint i)
{
    if (!gspec->item)
        gspec->item = g_array_new(FALSE, FALSE, sizeof(GwyToolboxItemSpec));

    if (i >= spec->group->len)
        g_array_append_vals(spec->group, gspec, 1);
    else
        g_array_insert_vals(spec->group, i, gspec, 1);
}

GwyToolboxSpec*
gwy_parse_toolbox_ui(gboolean ignore_user)
{
    GwyToolboxSpec *spec;
    GError *error = NULL;
    gchar *p, *q, *ui;
    gsize ui_len;

    p = g_build_filename(gwy_get_user_dir(), "ui", TOOLBOX_UI_FILE_NAME, NULL);
    if (ignore_user || !g_file_get_contents(p, &ui, &ui_len, NULL)) {
        g_free(p);
        q = gwy_find_self_dir("data");
        p = g_build_filename(q, "ui", TOOLBOX_UI_FILE_NAME, NULL);
        g_free(q);
        if (!g_file_get_contents(p, &ui, &ui_len, NULL)) {
            g_critical("Cannot find toolbox user interface %s", p);
            return NULL;
        }
    }
    g_free(p);

    spec = gwy_toolbox_parse(ui, ui_len, &error);
    g_free(ui);

    if (!spec) {
        g_critical("Cannot parse %s: %s", TOOLBOX_UI_FILE_NAME, error->message);
        return NULL;
    }

    return spec;
}

GwyToolboxSpec*
gwy_toolbox_spec_duplicate(GwyToolboxSpec *spec)
{
    GwyToolboxSpec *dup;
    guint i, j;

    dup = g_new0(GwyToolboxSpec, 1);
    dup->width = spec->width;
    dup->seen_tool_placeholder = spec->seen_tool_placeholder;
    dup->group = g_array_new(FALSE, FALSE, sizeof(GwyToolboxGroupSpec));
    for (i = 0; i < spec->group->len; i++) {
        GwyToolboxGroupSpec *gspec = &g_array_index(spec->group,
                                                    GwyToolboxGroupSpec, i);
        GwyToolboxGroupSpec gdup;

        gdup.item = g_array_new(FALSE, FALSE, sizeof(GwyToolboxItemSpec));
        gdup.name = g_strdup(gspec->name);
        gdup.id = gspec->id;
        g_array_append_val(dup->group, gdup);

        for (j = 0; j < gspec->item->len; j++) {
            GwyToolboxItemSpec *ispec = &g_array_index(gspec->item,
                                                       GwyToolboxItemSpec, j);
            GwyToolboxItemSpec idup;

            idup.type = ispec->type;
            idup.function = ispec->function;
            idup.icon = ispec->icon;
            idup.mode = ispec->mode;
            g_array_append_val(gdup.item, idup);
        }
    }

    return dup;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
