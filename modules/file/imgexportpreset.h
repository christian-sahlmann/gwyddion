/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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

#define GWY_TYPE_IMG_EXPORT_PRESET             (gwy_img_export_preset_get_type())
#define GWY_IMG_EXPORT_PRESET(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_IMG_EXPORT_PRESET, GwyImgExportPreset))
#define GWY_IMG_EXPORT_PRESET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_IMG_EXPORT_PRESET, GwyImgExportPresetClass))
#define GWY_IS_IMG_EXPORT_PRESET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_IMG_EXPORT_PRESET))
#define GWY_IS_IMG_EXPORT_PRESET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_IMG_EXPORT_PRESET))
#define GWY_IMG_EXPORT_PRESET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_IMG_EXPORT_PRESET, GwyImgExportPresetClass))

#define GWYRGBA_BLACK { 0.0, 0.0, 0.0, 1.0 }
#define GWYRGBA_WHITE { 1.0, 1.0, 1.0, 1.0 }

enum {
    NPAGES = 5,
};

typedef enum {
    IMGEXPORT_MODE_PRESENTATION,
    IMGEXPORT_MODE_GREY16,
} ImgExportMode;

typedef enum {
    IMGEXPORT_LATERAL_NONE,
    IMGEXPORT_LATERAL_RULERS,
    IMGEXPORT_LATERAL_INSET,
    IMGEXPORT_LATERAL_NTYPES
} ImgExportLateralType;

typedef enum {
    IMGEXPORT_VALUE_NONE,
    IMGEXPORT_VALUE_FMSCALE,
    IMGEXPORT_VALUE_NTYPES
} ImgExportValueType;

typedef enum {
    IMGEXPORT_TITLE_NONE,
    IMGEXPORT_TITLE_TOP,
    IMGEXPORT_TITLE_FMSCALE,
    IMGEXPORT_TITLE_NTYPES
} ImgExportTitleType;

typedef enum {
    INSET_POS_TOP_LEFT,
    INSET_POS_TOP_CENTER,
    INSET_POS_TOP_RIGHT,
    INSET_POS_BOTTOM_LEFT,
    INSET_POS_BOTTOM_CENTER,
    INSET_POS_BOTTOM_RIGHT,
    INSET_NPOS
} InsetPosType;

typedef struct {
    gdouble font_size;
    gdouble line_width;
    gdouble outline_width;
    gdouble border_width;
    gdouble tick_length;
} SizeSettings;

typedef struct _ImgExportEnv ImgExportEnv;

typedef struct {
    /* env, preset_name and active_page are only meaningful for settings, not
     * presets. */
    ImgExportEnv *env;
    gchar *preset_name;
    guint active_page;
    ImgExportMode mode;
    gdouble pxwidth;       /* Pixel width in mm for vector. */
    gdouble zoom;          /* Pixelwise for pixmaps. */
    SizeSettings sizes;
    ImgExportLateralType xytype;
    ImgExportValueType ztype;
    GwyRGBA inset_color;
    GwyRGBA inset_outline_color;
    InsetPosType inset_pos;
    gboolean draw_mask;
    gboolean draw_frame;
    gboolean draw_selection;
    gchar *font;
    gboolean scale_font;   /* TRUE = font size tied to data pixels */
    gboolean inset_draw_ticks;
    gboolean inset_draw_label;
    gdouble fmscale_gap;
    gdouble inset_xgap;
    gdouble inset_ygap;
    gdouble title_gap;
    gchar *inset_length;
    GwyInterpolationType interpolation;
    ImgExportTitleType title_type;
    gboolean units_in_title;
    gchar *selection;
    GwyRGBA sel_color;
    GwyRGBA sel_outline_color;
    /* Selection-specific options.  If we find a layer displaying the
     * selection we try to oppostuntistically init them from the layer.  */
    gboolean sel_number_objects;
    gdouble sel_line_thickness;
    gdouble sel_point_radius;
} ImgExportArgs;

typedef struct _GwyImgExportPreset      GwyImgExportPreset;
typedef struct _GwyImgExportPresetClass GwyImgExportPresetClass;

struct _GwyImgExportPreset {
    GwyResource parent_instance;
    ImgExportArgs data;
};

struct _GwyImgExportPresetClass {
    GwyResourceClass parent_class;
};

static GType               gwy_img_export_preset_get_type(void)                      G_GNUC_CONST;
static void                gwy_img_export_preset_finalize(GObject *object);
static GwyImgExportPreset* gwy_img_export_preset_new     (const gchar *name,
                                                          const ImgExportArgs *data,
                                                          gboolean is_const);
static void                gwy_img_export_preset_dump    (GwyResource *resource,
                                                          GString *str);
static GwyResource*        gwy_img_export_preset_parse   (const gchar *text,
                                                          gboolean is_const);


static const ImgExportArgs img_export_defaults = {
    NULL, NULL, 0,
    IMGEXPORT_MODE_PRESENTATION,
    0.1, 1.0,
    { 12.0, 1.0, 0.0, 0.0, 10.0 },
    IMGEXPORT_LATERAL_RULERS, IMGEXPORT_VALUE_FMSCALE,
    GWYRGBA_WHITE, GWYRGBA_WHITE, INSET_POS_BOTTOM_RIGHT,
    TRUE, TRUE, FALSE,
    "Helvetica", TRUE, TRUE, TRUE,
    1.0, 1.0, 1.0, 0.0, "",
    GWY_INTERPOLATION_ROUND,
    IMGEXPORT_TITLE_NONE, FALSE,
    "line", GWYRGBA_WHITE, GWYRGBA_WHITE,
    TRUE, 0.0, 0.0,
};

G_DEFINE_TYPE(GwyImgExportPreset, gwy_img_export_preset, GWY_TYPE_RESOURCE)

static void
gwy_img_export_preset_class_init(GwyImgExportPresetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    gobject_class->finalize = gwy_img_export_preset_finalize;

    parent_class = GWY_RESOURCE_CLASS(gwy_img_export_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);

    res_class->name = "imgexport";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    res_class->dump = gwy_img_export_preset_dump;
    res_class->parse = gwy_img_export_preset_parse;
}

static void
img_export_unconst_args(ImgExportArgs *args)
{
    args->font = g_strdup(args->font);
    args->inset_length = g_strdup(args->inset_length);
    args->selection = g_strdup(args->selection);
    args->preset_name = g_strdup(args->preset_name);
}

static void
gwy_img_export_preset_init(GwyImgExportPreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
    preset->data = img_export_defaults;
    img_export_unconst_args(&preset->data);
}

static void
img_export_free_args(ImgExportArgs *args)
{
    g_free(args->font);
    g_free(args->inset_length);
    g_free(args->selection);
    g_free(args->preset_name);
}

static void
gwy_img_export_preset_finalize(GObject *object)
{
    GwyImgExportPreset *preset;

    preset = GWY_IMG_EXPORT_PRESET(object);
    img_export_free_args(&preset->data);
    G_OBJECT_CLASS(gwy_img_export_preset_parent_class)->finalize(object);
}

static void
img_export_sanitize_args(ImgExportArgs *args)
{
    if (args->mode != IMGEXPORT_MODE_GREY16)
        args->mode = IMGEXPORT_MODE_PRESENTATION;
    args->active_page = MIN(args->active_page, NPAGES-1);
    args->xytype = MIN(args->xytype, IMGEXPORT_LATERAL_NTYPES-1);
    args->ztype = MIN(args->ztype, IMGEXPORT_VALUE_NTYPES-1);
    args->inset_pos = MIN(args->inset_pos, INSET_NPOS-1);
    args->interpolation = gwy_enum_sanitize_value(args->interpolation,
                                                  GWY_TYPE_INTERPOLATION_TYPE);
    args->title_type = MIN(args->title_type, IMGEXPORT_TITLE_NTYPES-1);
    /* handle inset_length later, its usability depends on the data field. */
    args->zoom = CLAMP(args->zoom, 0.06, 16.0);
    args->pxwidth = CLAMP(args->pxwidth, 0.01, 25.4);
    args->draw_mask = !!args->draw_mask;
    args->draw_frame = !!args->draw_frame;
    args->draw_selection = !!args->draw_selection;
    args->scale_font = !!args->scale_font;
    args->inset_draw_ticks = !!args->inset_draw_ticks;
    args->inset_draw_label = !!args->inset_draw_label;
    args->units_in_title = !!args->units_in_title;
    args->sizes.font_size = CLAMP(args->sizes.font_size, 1.0, 1024.0);
    args->sizes.line_width = CLAMP(args->sizes.line_width, 0.0, 16.0);
    args->sizes.outline_width = CLAMP(args->sizes.outline_width, 0.0, 16.0);
    args->sizes.border_width = CLAMP(args->sizes.border_width, 0.0, 1024.0);
    args->sizes.tick_length = CLAMP(args->sizes.tick_length, 0.0, 120.0);
    args->fmscale_gap = CLAMP(args->fmscale_gap, 0.0, 2.0);
    args->inset_xgap = CLAMP(args->inset_xgap, 0.0, 4.0);
    args->inset_ygap = CLAMP(args->inset_ygap, 0.0, 2.0);
    args->title_gap = CLAMP(args->title_gap, -1.0, 1.0);
    args->inset_outline_color.a = args->inset_color.a;
    args->sel_outline_color.a = args->sel_color.a;
    args->sel_number_objects = !!args->sel_number_objects;
    args->sel_line_thickness = CLAMP(args->sel_line_thickness, 0.0, 1024.0);
    args->sel_point_radius = CLAMP(args->sel_point_radius, 0.0, 1024.0);
}

static void
gwy_img_export_preset_data_copy(const ImgExportArgs *src,
                                ImgExportArgs *dest)
{
    ImgExportEnv *env = dest->env;
    gchar *preset_name = dest->preset_name;
    gchar *selection = dest->selection;
    guint active_page = dest->active_page;

    g_return_if_fail(src != (const ImgExportArgs*)dest);
    dest->preset_name = NULL;
    dest->selection = NULL;
    img_export_free_args(dest);

    *dest = *src;
    img_export_unconst_args(dest);

    g_free(dest->preset_name);
    g_free(dest->selection);
    dest->env = env;
    dest->preset_name = preset_name;
    dest->selection = selection;
    dest->active_page = active_page;
}

static GwyImgExportPreset*
gwy_img_export_preset_new(const gchar *name,
                          const ImgExportArgs *data,
                          gboolean is_const)
{
    GwyImgExportPreset *preset;

    preset = g_object_new(GWY_TYPE_IMG_EXPORT_PRESET,
                          "is-const", is_const,
                          NULL);
    gwy_img_export_preset_data_copy(data, &preset->data);
    g_string_assign(GWY_RESOURCE(preset)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(preset)->is_modified = !is_const;

    return preset;
}

static void
dump_rgba(const GwyRGBA *rgba, gchar *r, gchar *g, gchar *b, gchar *a)
{
    g_ascii_formatd(r, G_ASCII_DTOSTR_BUF_SIZE, "%.6g", rgba->r);
    g_ascii_formatd(g, G_ASCII_DTOSTR_BUF_SIZE, "%.6g", rgba->g);
    g_ascii_formatd(b, G_ASCII_DTOSTR_BUF_SIZE, "%.6g", rgba->b);
    g_ascii_formatd(a, G_ASCII_DTOSTR_BUF_SIZE, "%.6g", rgba->a);
}

static void
gwy_img_export_preset_dump(GwyResource *resource,
                           GString *str)
{
    gchar d1[G_ASCII_DTOSTR_BUF_SIZE], d2[G_ASCII_DTOSTR_BUF_SIZE],
          d3[G_ASCII_DTOSTR_BUF_SIZE], d4[G_ASCII_DTOSTR_BUF_SIZE],
          d5[G_ASCII_DTOSTR_BUF_SIZE];
    GwyImgExportPreset *preset;
    ImgExportArgs *data;
    gchar *s;

    g_return_if_fail(GWY_IS_IMG_EXPORT_PRESET(resource));
    preset = GWY_IMG_EXPORT_PRESET(resource);
    data = &preset->data;

    g_ascii_dtostr(d1, sizeof(d1), data->pxwidth);
    g_ascii_dtostr(d2, sizeof(d2), data->zoom);
    s = g_strescape(data->font, NULL);
    g_string_append_printf(str,
                           "mode %u\n"
                           "pxwidth %s\n"
                           "zoom %s\n"
                           "scale_font %d\n"
                           "xytype %u\n"
                           "ztype %u\n"
                           "inset_pos %u\n"
                           "draw_mask %d\n"
                           "draw_frame %d\n"
                           "draw_selection %d\n"
                           "font \"%s\"\n",
                           data->mode, d1, d2, data->scale_font,
                           data->xytype, data->ztype, data->inset_pos,
                           data->draw_mask, data->draw_frame,
                           data->draw_selection, s);
    g_free(s);

    g_ascii_dtostr(d1, sizeof(d1), data->sizes.font_size);
    g_ascii_dtostr(d2, sizeof(d2), data->sizes.line_width);
    g_ascii_dtostr(d3, sizeof(d3), data->sizes.outline_width);
    g_ascii_dtostr(d4, sizeof(d4), data->sizes.border_width);
    g_ascii_dtostr(d5, sizeof(d5), data->sizes.tick_length);
    g_string_append_printf(str,
                           "font_size %s\n"
                           "line_width %s\n"
                           "outline_width %s\n"
                           "border_width %s\n"
                           "tick_length %s\n",
                           d1, d2, d3, d4, d5);

    dump_rgba(&data->inset_color, d1, d2, d3, d4);
    g_string_append_printf(str, "inset_color %s %s %s\n", d1, d2, d3);
    dump_rgba(&data->inset_outline_color, d1, d2, d3, d5);
    g_string_append_printf(str, "inset_outline_color %s %s %s\n", d1, d2, d3);
    g_string_append_printf(str, "inset_opacity %s\n", d4);

    g_ascii_dtostr(d1, sizeof(d1), data->fmscale_gap);
    g_ascii_dtostr(d2, sizeof(d2), data->inset_xgap);
    g_ascii_dtostr(d3, sizeof(d3), data->inset_ygap);
    g_ascii_dtostr(d4, sizeof(d4), data->title_gap);
    g_string_append_printf(str,
                           "fmscale_gap %s\n"
                           "inset_xgap %s\n"
                           "inset_ygap %s\n"
                           "title_gap %s\n",
                           d1, d2, d3, d4);

    s = g_strescape(data->inset_length, NULL);
    g_string_append_printf(str,
                           "inset_length %s\n"
                           "interpolation %u\n"
                           "title_type %u\n"
                           "units_in_title %d\n",
                           s, data->interpolation, data->title_type,
                           data->units_in_title);
    g_free(s);

    s = g_strescape(data->selection, NULL);
    g_ascii_dtostr(d1, sizeof(d1), data->sel_line_thickness);
    g_ascii_dtostr(d2, sizeof(d2), data->sel_point_radius);
    g_string_append_printf(str,
                           "selection %s\n"
                           "sel_number_objects %d\n"
                           "sel_line_thickness %s\n"
                           "sel_point_radius %s\n",
                           s, data->sel_number_objects, d1, d2);
    g_free(s);

    dump_rgba(&data->sel_color, d1, d2, d3, d4);
    g_string_append_printf(str, "sel_color %s %s %s\n", d1, d2, d3);
    dump_rgba(&data->sel_outline_color, d1, d2, d3, d5);
    g_string_append_printf(str, "sel_outline_color %s %s %s\n", d1, d2, d3);
    g_string_append_printf(str, "sel_opacity %s\n", d4);
}

static void
parse_rgb(GwyRGBA *rgba, gchar *s)
{
    gdouble r, g, b;
    gchar *end;

    r = g_ascii_strtod(s, &end);
    if (end == s)
        return;
    s = end;

    g = g_ascii_strtod(s, &end);
    if (end == s)
        return;
    s = end;

    b = g_ascii_strtod(s, &end);
    if (end == s)
        return;

    rgba->r = CLAMP(r, 0.0, 1.0);
    rgba->g = CLAMP(g, 0.0, 1.0);
    rgba->b = CLAMP(b, 0.0, 1.0);
}

static GwyResource*
gwy_img_export_preset_parse(const gchar *text,
                            gboolean is_const)
{
    GwyImgExportPresetClass *klass;
    GwyImgExportPreset *preset = NULL;
    ImgExportArgs data;
    gchar *str, *p, *line, *key, *value;
    guint len;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_IMG_EXPORT_PRESET);
    g_return_val_if_fail(klass, NULL);

    data = img_export_defaults;
    img_export_unconst_args(&data);

    p = str = g_strdup(text);
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        key = line;
        if (!*key)
            continue;
        value = strchr(key, ' ');
        if (value) {
            *value = '\0';
            value++;
            g_strstrip(value);
        }
        if (!value || !*value) {
            g_warning("Missing value for `%s'.", key);
            continue;
        }

        if (gwy_strequal(key, "mode"))
            data.mode = atoi(value);
        else if (gwy_strequal(key, "xytype"))
            data.xytype = atoi(value);
        else if (gwy_strequal(key, "ztype"))
            data.ztype = atoi(value);
        else if (gwy_strequal(key, "inset_pos"))
            data.inset_pos = atoi(value);
        else if (gwy_strequal(key, "interpolation"))
            data.interpolation = atoi(value);
        else if (gwy_strequal(key, "title_type"))
            data.title_type = atoi(value);
        else if (gwy_strequal(key, "draw_mask"))
            data.draw_mask = atoi(value);
        else if (gwy_strequal(key, "draw_frame"))
            data.draw_frame = atoi(value);
        else if (gwy_strequal(key, "draw_selection"))
            data.draw_selection = atoi(value);
        else if (gwy_strequal(key, "scale_font"))
            data.scale_font = atoi(value);
        else if (gwy_strequal(key, "inset_draw_ticks"))
            data.inset_draw_ticks = atoi(value);
        else if (gwy_strequal(key, "inset_draw_label"))
            data.inset_draw_label = atoi(value);
        else if (gwy_strequal(key, "units_in_title"))
            data.units_in_title = atoi(value);
        else if (gwy_strequal(key, "sel_number_objects"))
            data.sel_number_objects = atoi(value);
        else if (gwy_strequal(key, "pxwidth"))
            data.pxwidth = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "zoom"))
            data.zoom = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "font_size"))
            data.sizes.font_size = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "tick_length"))
            data.sizes.tick_length = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "line_width"))
            data.sizes.line_width = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "outline_width"))
            data.sizes.outline_width = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "border_width"))
            data.sizes.border_width = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "fmscale_gap"))
            data.fmscale_gap = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "inset_xgap"))
            data.inset_xgap = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "inset_ygap"))
            data.inset_ygap = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "title_gap"))
            data.title_gap = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "sel_line_thickness"))
            data.sel_line_thickness = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "sel_point_radius"))
            data.sel_point_radius = g_ascii_strtod(value, NULL);
        else if (gwy_strequal(key, "selection")) {
            len = strlen(value);
            if (value[0] == '"' && len >= 2 && value[len-1] == '"') {
                value[len-1] = '\0';
                value++;
                g_free(data.selection);
                data.selection = g_strcompress(value);
            }
        }
        else if (gwy_strequal(key, "font")) {
            len = strlen(value);
            if (value[0] == '"' && len >= 2 && value[len-1] == '"') {
                value[len-1] = '\0';
                value++;
                g_free(data.font);
                data.font = g_strcompress(value);
            }
        }
        else if (gwy_strequal(key, "inset_length")) {
            len = strlen(value);
            if (value[0] == '"' && len >= 2 && value[len-1] == '"') {
                value[len-1] = '\0';
                value++;
                g_free(data.inset_length);
                data.inset_length = g_strcompress(value);
            }
        }
        else if (gwy_strequal(key, "inset_opacity")) {
            gdouble alpha = g_ascii_strtod(value, NULL);
            alpha = CLAMP(alpha, 0.0, 1.0);
            data.inset_color.a = alpha;
            data.inset_outline_color.a = alpha;
        }
        else if (gwy_strequal(key, "sel_opacity")) {
            gdouble alpha = g_ascii_strtod(value, NULL);
            alpha = CLAMP(alpha, 0.0, 1.0);
            data.sel_color.a = alpha;
            data.sel_outline_color.a = alpha;
        }
        else if (gwy_strequal(key, "inset_color"))
            parse_rgb(&data.inset_color, value);
        else if (gwy_strequal(key, "inset_outline_color"))
            parse_rgb(&data.inset_outline_color, value);
        else if (gwy_strequal(key, "sel_color"))
            parse_rgb(&data.sel_color, value);
        else if (gwy_strequal(key, "sel_outline_color"))
            parse_rgb(&data.sel_outline_color, value);
        else
            g_warning("Unknown field `%s'.", key);
    }

    preset = gwy_img_export_preset_new("", &data, is_const);
    GWY_RESOURCE(preset)->is_modified = FALSE;
    img_export_sanitize_args(&preset->data);
    img_export_free_args(&data);
    g_free(str);

    return (GwyResource*)preset;
}

static GwyInventory*
gwy_img_export_presets(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek
                                      (GWY_TYPE_IMG_EXPORT_PRESET))->inventory;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
