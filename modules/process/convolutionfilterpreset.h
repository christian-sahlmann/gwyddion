/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

enum {
    CONVOLUTION_MIN_SIZE = 3,
    CONVOLUTION_MAX_SIZE = 9
};

#define GWY_TYPE_CONVOLUTION_FILTER_PRESET             (gwy_convolution_filter_preset_get_type())
#define GWY_CONVOLUTION_FILTER_PRESET(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CONVOLUTION_FILTER_PRESET, GwyConvolutionFilterPreset))
#define GWY_CONVOLUTION_FILTER_PRESET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CONVOLUTION_FILTER_PRESET, GwyConvolutionFilterPresetClass))
#define GWY_IS_CONVOLUTION_FILTER_PRESET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CONVOLUTION_FILTER_PRESET))
#define GWY_IS_CONVOLUTION_FILTER_PRESET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CONVOLUTION_FILTER_PRESET))
#define GWY_CONVOLUTION_FILTER_PRESET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CONVOLUTION_FILTER_PRESET, GwyConvolutionFilterPresetClass))

typedef struct _GwyConvolutionFilterPreset      GwyConvolutionFilterPreset;
typedef struct _GwyConvolutionFilterPresetClass GwyConvolutionFilterPresetClass;

typedef struct {
    guint size;
    gdouble divisor;
    gboolean auto_divisor;
    gdouble *matrix;
} GwyConvolutionFilterPresetData;

struct _GwyConvolutionFilterPreset {
    GwyResource parent_instance;
    GwyConvolutionFilterPresetData data;
};

struct _GwyConvolutionFilterPresetClass {
    GwyResourceClass parent_class;
};

static GType       gwy_convolution_filter_preset_get_type (void) G_GNUC_CONST;
static void        gwy_convolution_filter_preset_finalize (GObject *object);
static void        gwy_convolution_filter_preset_data_copy(const GwyConvolutionFilterPresetData *src,
                                                           GwyConvolutionFilterPresetData *dest);
static GwyConvolutionFilterPreset* gwy_convolution_filter_preset_new(const gchar *name,
                                          const GwyConvolutionFilterPresetData *data,
                                          gboolean is_const);
static void           gwy_convolution_filter_preset_dump  (GwyResource *resource,
                                                    GString *str);
static GwyResource*   gwy_convolution_filter_preset_parse (const gchar *text,
                                                    gboolean is_const);


static gdouble convolution_identity[] = {
    0.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 0.0,
};

static const GwyConvolutionFilterPresetData convolutionpresetdata_default = {
    3, 1.0, TRUE, convolution_identity,
};

G_DEFINE_TYPE(GwyConvolutionFilterPreset,
              gwy_convolution_filter_preset,
              GWY_TYPE_RESOURCE)

static void
gwy_convolution_filter_preset_class_init(GwyConvolutionFilterPresetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    gobject_class->finalize = gwy_convolution_filter_preset_finalize;

    parent_class
        = GWY_RESOURCE_CLASS(gwy_convolution_filter_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);

    res_class->name = "convolutionfilter";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    res_class->dump = gwy_convolution_filter_preset_dump;
    res_class->parse = gwy_convolution_filter_preset_parse;
}

static void
gwy_convolution_filter_preset_init(GwyConvolutionFilterPreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
    gwy_convolution_filter_preset_data_copy(&convolutionpresetdata_default,
                                            &preset->data);
}

static void
gwy_convolution_filter_preset_finalize(GObject *object)
{
    GwyConvolutionFilterPreset *preset;

    preset = GWY_CONVOLUTION_FILTER_PRESET(object);
    g_free(preset->data.matrix);

    G_OBJECT_CLASS(gwy_convolution_filter_preset_parent_class)->finalize(object);
}

static inline gboolean
gwy_convolution_filter_preset_check_size(guint size)
{
    return size >= CONVOLUTION_MIN_SIZE
           && size <= CONVOLUTION_MAX_SIZE
           && (size & 1);
}

static void
gwy_convolution_filter_preset_data_autodiv(GwyConvolutionFilterPresetData *data)
{
    gdouble max, sum;
    guint i;

    max = sum = 0.0;
    for (i = 0; i < data->size*data->size; i++) {
        sum += data->matrix[i];
        max = MAX(max, fabs(data->matrix[i]));
    }

    /* zero: values are entered by user with limited precision */
    if (fabs(sum) <= 1e-6*max)
        data->divisor = 1.0;
    else
        data->divisor = sum;
}

static void
gwy_convolution_filter_preset_data_resize(GwyConvolutionFilterPresetData *data,
                                          guint newsize)
{
    gdouble *oldmatrix;
    guint i, d;

    g_return_if_fail(gwy_convolution_filter_preset_check_size(newsize));
    if (newsize == data->size)
        return;

    oldmatrix = data->matrix;
    data->matrix = g_new0(gdouble, newsize*newsize);
    if (newsize < data->size) {
        d = (data->size - newsize)/2;
        for (i = 0; i < newsize; i++)
            memcpy(data->matrix + i*newsize,
                   oldmatrix + (i + d)*data->size + d,
                   newsize*sizeof(gdouble));
    }
    else {
        d = (newsize - data->size)/2;
        for (i = 0; i < data->size; i++)
            memcpy(data->matrix + (i + d)*newsize + d,
                   oldmatrix + i*data->size,
                   data->size*sizeof(gdouble));
    }
    data->size = newsize;
    g_free(oldmatrix);

    if (data->auto_divisor)
        gwy_convolution_filter_preset_data_autodiv(data);
}

static void
gwy_convolution_filter_preset_data_sanitize(GwyConvolutionFilterPresetData *data)
{
    /* Simply replace the filter with default when it't really weird */
    if (!gwy_convolution_filter_preset_check_size(data->size)) {
        gwy_convolution_filter_preset_data_copy(&convolutionpresetdata_default,
                                                data);
        return;
    }

    if (!data->divisor)
        data->auto_divisor = TRUE;

    data->auto_divisor = !!data->auto_divisor;
    if (data->auto_divisor)
        gwy_convolution_filter_preset_data_autodiv(data);
}

static void
gwy_convolution_filter_preset_data_copy(const GwyConvolutionFilterPresetData *src,
                                        GwyConvolutionFilterPresetData *dest)
{
    g_free(dest->matrix);
    *dest = *src;
    dest->matrix = g_memdup(src->matrix, src->size*src->size*sizeof(gdouble));
}

static GwyConvolutionFilterPreset*
gwy_convolution_filter_preset_new(const gchar *name,
                                  const GwyConvolutionFilterPresetData *data,
                                  gboolean is_const)
{
    GwyConvolutionFilterPreset *preset;

    preset = g_object_new(GWY_TYPE_CONVOLUTION_FILTER_PRESET,
                          "is-const", is_const,
                          NULL);
    gwy_convolution_filter_preset_data_copy(data, &preset->data);
    g_string_assign(GWY_RESOURCE(preset)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(preset)->is_modified = !is_const;

    return preset;
}

static void
gwy_convolution_filter_preset_dump(GwyResource *resource,
                                   GString *str)
{
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    GwyConvolutionFilterPreset *preset;
    guint i;

    g_return_if_fail(GWY_IS_CONVOLUTION_FILTER_PRESET(resource));
    preset = GWY_CONVOLUTION_FILTER_PRESET(resource);

    /* Information */
    g_ascii_dtostr(buf, sizeof(buf), preset->data.divisor);
    g_string_append_printf(str,
                           "size %u\n"
                           "divisor %s\n"
                           "auto_divisor %d\n",
                           preset->data.size,
                           buf,
                           preset->data.auto_divisor);
    for (i = 0; i < preset->data.size; i++) {
        g_ascii_formatd(buf, sizeof(buf), "%.8g", preset->data.matrix[i]);
        g_string_append(str, buf);
        if ((i + 1) % preset->data.size == 0)
            g_string_append_c(str, '\n');
        else
            g_string_append_c(str, ' ');
    }
}

static GwyResource*
gwy_convolution_filter_preset_parse(const gchar *text,
                                    gboolean is_const)
{
    GwyConvolutionFilterPresetData data;
    GwyConvolutionFilterPreset *preset = NULL;
    GwyConvolutionFilterPresetClass *klass;
    gchar *str, *p, *line, *key, *value, *end;
    guint i;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_CONVOLUTION_FILTER_PRESET);
    g_return_val_if_fail(klass, NULL);

    data.divisor = 1.0;
    data.auto_divisor = TRUE;
    data.size = 0;
    data.matrix = NULL;

    p = str = g_strdup(text);
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            continue;

        /* Data start */
        if (g_ascii_isdigit(line[0])
            || line[0] == '.' || line[0] == '-' || line[0] == '+')
            break;

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

        if (gwy_strequal(key, "size"))
            data.size = atoi(value);
        else if (gwy_strequal(key, "auto_divisor"))
            data.auto_divisor = !!atoi(value);
        else if (gwy_strequal(key, "xreal"))
            data.divisor = g_ascii_strtod(value, NULL);
        else
            g_warning("Unknown field `%s'.", key);
    }

    if (!gwy_convolution_filter_preset_check_size(data.size)) {
        g_free(str);
        return NULL;
    }

    data.matrix = g_new0(gdouble, data.size*data.size);
    for (i = 0; i < data.size*data.size; i++) {
        if (!(line = gwy_str_next_line(&p)))
            break;
        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            continue;

        data.matrix[i] = g_ascii_strtod(line, &end);
        if (end == line)
            break;
    }
    g_free(str);

    if (i != data.size*data.size) {
        g_free(data.matrix);
        return NULL;
    }

    preset = gwy_convolution_filter_preset_new("", &data, is_const);
    gwy_convolution_filter_preset_data_sanitize(&preset->data);

    return (GwyResource*)preset;
}

static GwyInventory*
gwy_convolution_filter_presets(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek
                               (GWY_TYPE_CONVOLUTION_FILTER_PRESET))->inventory;
}
