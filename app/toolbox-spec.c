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
#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>
#include <app/gwyapp.h>

#include "gwyddion.h"
#include "toolbox.h"

#define TOOLBOX_UI_FILE_NAME "toolbox.xml"
#define TOOLBOX_INDENT "  "

static const GwyEnum modes[] = {
    { "default",         0,                   },
    { "interactive",     GWY_RUN_INTERACTIVE, },
    { "non-interactive", GWY_RUN_IMMEDIATE,   },
};

static const GwyEnum action_types[] = {
    { "empty",   GWY_APP_ACTION_TYPE_PLACEHOLDER, },
    { "builtin", GWY_APP_ACTION_TYPE_BUILTIN,     },
    { "proc",    GWY_APP_ACTION_TYPE_PROC,        },
    { "graph",   GWY_APP_ACTION_TYPE_GRAPH,       },
    { "volume",  GWY_APP_ACTION_TYPE_VOLUME,      },
    { "xyz",     GWY_APP_ACTION_TYPE_XYZ,         },
    { "tool",    GWY_APP_ACTION_TYPE_TOOL,        },
};

static GwyToolboxSpec* gwy_toolbox_parse              (const gchar *ui,
                                                       gsize ui_len,
                                                       GError **error);
static void            gwy_app_menu_canonicalize_label(gchar *label);

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
                && (tmptype = gwy_toolbox_find_action_type(attval)) != -1)
                type = tmptype;
            else if (gwy_strequal(attname, "run")
                     && (tmpmode = gwy_toolbox_find_mode(attval))
                        != (GwyRunType)-1)
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

static GwyToolboxSpec*
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
    GwyToolboxItemSpec *ispec;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);
    g_return_if_fail(j < gspec->item->len);
    ispec = &g_array_index(gspec->item, GwyToolboxItemSpec, j);
    if (ispec->type == GWY_APP_ACTION_TYPE_TOOL && !ispec->function) {
        g_assert(spec->seen_tool_placeholder);
        spec->seen_tool_placeholder = FALSE;
    }
    g_array_remove_index(gspec->item, j);
}

void
gwy_toolbox_spec_remove_group(GwyToolboxSpec *spec, guint i)
{
    GwyToolboxGroupSpec *gspec;
    GwyToolboxItemSpec *ispec;
    guint j;

    g_return_if_fail(i < spec->group->len);
    gspec = &g_array_index(spec->group, GwyToolboxGroupSpec, i);
    g_free(gspec->name);
    if (gspec->item) {
        for (j = 0; j < gspec->item->len; j++) {
            ispec = &g_array_index(gspec->item, GwyToolboxItemSpec, j);
            if (ispec->type == GWY_APP_ACTION_TYPE_TOOL && !ispec->function) {
                g_assert(spec->seen_tool_placeholder);
                spec->seen_tool_placeholder = FALSE;
            }
        }
        g_array_free(gspec->item, TRUE);
    }
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

    if (ispec->type == GWY_APP_ACTION_TYPE_TOOL && !ispec->function) {
        g_assert(!spec->seen_tool_placeholder);
        spec->seen_tool_placeholder = TRUE;
    }

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

gboolean
gwy_save_toolbox_ui(GwyToolboxSpec *spec, GError **error)
{
    GwyToolboxGroupSpec *gspec;
    GwyToolboxItemSpec *ispec;
    GArray *group, *item;
    guint i, j;
    GString *xml;
    FILE *fh;
    gchar *filename;

    if (!gwy_app_settings_create_config_dir(error))
        return FALSE;

    xml = g_string_new("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    g_string_append_printf(xml, "<toolbox width='%u'>\n", spec->width);

    group = spec->group;
    for (i = 0; i < group->len; i++) {
        gspec = &g_array_index(group, GwyToolboxGroupSpec, i);
        g_string_append_printf(xml, "%s<group id='%s' title='%s'",
                               TOOLBOX_INDENT,
                               g_quark_to_string(gspec->id), gspec->name);
        if (gspec->translatable)
            g_string_append(xml, " translatable='yes'");
        g_string_append(xml, ">\n");

        item = gspec->item;
        for (j = 0; j < item->len; j++) {
            ispec = &g_array_index(item, GwyToolboxItemSpec, j);
            g_string_append_printf(xml, "%s<item type='%s'",
                                   TOOLBOX_INDENT TOOLBOX_INDENT,
                                   gwy_toolbox_action_type_name(ispec->type));
            if (ispec->function) {
                g_string_append_printf(xml, " function='%s'",
                                       g_quark_to_string(ispec->function));
            }
            if (ispec->icon) {
                g_string_append_printf(xml, " icon='%s'",
                                       g_quark_to_string(ispec->icon));
            }
            if (ispec->mode) {
                g_string_append_printf(xml, " run='%s'",
                                       gwy_toolbox_mode_name(ispec->mode));
            }
            g_string_append(xml, "/>\n");
        }
        g_string_append(xml, TOOLBOX_INDENT "</group>\n");
    }
    g_string_append(xml, "</toolbox>\n");

    filename = g_build_filename(gwy_get_user_dir(), "ui", TOOLBOX_UI_FILE_NAME,
                                NULL);
    fh = gwy_fopen(filename, "w");
    g_free(filename);
    if (!fh) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                    _("Cannot open file for writing: %s."),
                    g_strerror(errno));
        g_string_free(xml, TRUE);
        return FALSE;
    }
    if (fwrite(xml->str, 1, xml->len, fh) != xml->len) {
        g_set_error(error,
                    GWY_APP_SETTINGS_ERROR, GWY_APP_SETTINGS_ERROR_FILE,
                    _("Cannot write to file: %s."), g_strerror(errno));
        fclose(fh);
        g_string_free(xml, TRUE);
        return FALSE;
    }
    fclose(fh);
    g_string_free(xml, TRUE);

    return TRUE;
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

const gchar*
gwy_toolbox_action_type_name(GwyAppActionType type)
{
    if (type < 0)
        return NULL;

    return gwy_enum_to_string(type, action_types, G_N_ELEMENTS(action_types));
}

GwyAppActionType
gwy_toolbox_find_action_type(const gchar *name)
{
    return gwy_string_to_enum(name, action_types, G_N_ELEMENTS(action_types));
}

const gchar*
gwy_toolbox_mode_name(GwyRunType mode)
{
    return gwy_enum_to_string(mode, modes, G_N_ELEMENTS(modes));
}

GwyRunType
gwy_toolbox_find_mode(const gchar *name)
{
    return gwy_string_to_enum(name, modes, G_N_ELEMENTS(modes));
}

const gchar*
gwy_toolbox_action_nice_name(GwyAppActionType type, const gchar *name)
{
    static GString *label = NULL;

    const gchar *menupath = NULL;

    if (type == GWY_APP_ACTION_TYPE_PLACEHOLDER)
        return _("placeholder");

    if (type == GWY_APP_ACTION_TYPE_TOOL) {
        if (name) {
            GType gtype = g_type_from_name(name);
            GwyToolClass *tool_class = g_type_class_peek(gtype);
            return gwy_tool_class_get_title(tool_class);
        }
        return _("remaining tools");
    }
    if (type == GWY_APP_ACTION_TYPE_BUILTIN) {
        const GwyToolboxBuiltinSpec* spec;

        if ((spec = gwy_toolbox_find_builtin_spec(name)))
            return spec->nice_name;
    }

    if (type == GWY_APP_ACTION_TYPE_PROC)
        menupath = gwy_process_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_GRAPH)
        menupath = gwy_graph_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_VOLUME)
        menupath = gwy_volume_func_get_menu_path(name);
    else if (type == GWY_APP_ACTION_TYPE_XYZ)
        menupath = gwy_xyz_func_get_menu_path(name);

    if (menupath) {
        const gchar *p;
        gchar *s = g_strdup(menupath);

        if (!label)
            label = g_string_new(NULL);

        gwy_app_menu_canonicalize_label(s);
        p = strrchr(s, '/');
        g_string_assign(label, p ? p+1 : s);
        g_free(s);

        return label->str;
    }

    return NULL;
}

const gchar*
gwy_toolbox_action_stock_id(GwyAppActionType type, const gchar *name)
{
    if (!name)
        return NULL;
    if (type == GWY_APP_ACTION_TYPE_PROC)
        return gwy_process_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_GRAPH)
        return gwy_graph_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_VOLUME)
        return gwy_volume_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_XYZ)
        return gwy_xyz_func_get_stock_id(name);
    if (type == GWY_APP_ACTION_TYPE_TOOL && name) {
        if (name) {
            GType gtype = g_type_from_name(name);
            GwyToolClass *tool_class = g_type_class_peek(gtype);
            return gwy_tool_class_get_stock_id(tool_class);
        }
    }
    if (type == GWY_APP_ACTION_TYPE_BUILTIN) {
        const GwyToolboxBuiltinSpec* spec;

        if ((spec = gwy_toolbox_find_builtin_spec(name)))
            return spec->stock_id;
    }
    return NULL;
}

const gchar*
gwy_toolbox_action_detail(GwyAppActionType type, const gchar *name)
{
    if (!name)
        return NULL;
    if (type == GWY_APP_ACTION_TYPE_PROC)
        return gwy_process_func_get_tooltip(name);
    if (type == GWY_APP_ACTION_TYPE_GRAPH)
        return gwy_graph_func_get_tooltip(name);
    if (type == GWY_APP_ACTION_TYPE_VOLUME)
        return gwy_volume_func_get_tooltip(name);
    if (type == GWY_APP_ACTION_TYPE_XYZ)
        return gwy_xyz_func_get_tooltip(name);
    if (type == GWY_APP_ACTION_TYPE_TOOL && name) {
        if (name) {
            GType gtype = g_type_from_name(name);
            GwyToolClass *tool_class = g_type_class_peek(gtype);
            return gwy_tool_class_get_tooltip(tool_class);
        }
        return _("All tools not placed explicitly go here.");
    }
    if (type == GWY_APP_ACTION_TYPE_BUILTIN) {
        const GwyToolboxBuiltinSpec* spec;

        if ((spec = gwy_toolbox_find_builtin_spec(name)))
            return spec->tooltip;
    }
    return NULL;
}

GwyRunType
gwy_toolbox_action_run_modes(GwyAppActionType type, const gchar *name)
{
    if (!name)
        return 0;
    if (type == GWY_APP_ACTION_TYPE_PROC)
        return gwy_process_func_get_run_types(name);
    if (type == GWY_APP_ACTION_TYPE_VOLUME)
        return gwy_volume_func_get_run_types(name);
    if (type == GWY_APP_ACTION_TYPE_XYZ)
        return gwy_xyz_func_get_run_types(name);

    return 0;
}

/* Copied from menu.c */
static void
gwy_app_menu_canonicalize_label(gchar *label)
{
    guint i, j;

    for (i = j = 0; label[i]; i++) {
        label[j] = label[i];
        if (label[i] != '_' || label[i+1] == '_')
            j++;
    }
    /* If the label *ends* with an underscore, just kill it */
    label[j] = '\0';
    if (j >= 3 && label[j-3] == '.' && label[j-2] == '.' && label[j-1] == '.')
        label[j-3] = '\0';
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
