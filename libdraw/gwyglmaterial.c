/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libdraw/gwyglmaterial.h>

#define GWY_GL_MATERIAL_DEFAULT "OpenGL-Default"

#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.99999999*(1 << (BITS_PER_SAMPLE)))

static gpointer       gwy_gl_material_copy       (gpointer);
static void           gwy_gl_material_changed    (GwyGLMaterial *gl_material);
static GwyGLMaterial* gwy_gl_material_new        (const gchar *name,
                                                  const GwyRGBA *ambient,
                                                  const GwyRGBA *diffuse,
                                                  const GwyRGBA *specular,
                                                  const GwyRGBA *emission,
                                                  gdouble shininess,
                                                  gboolean is_const);
static void           gwy_gl_material_dump       (GwyResource *resource,
                                                  GString *str);
static GwyResource*   gwy_gl_material_parse      (const gchar *text,
                                                  gboolean is_const);


/* OpenGL reference states these: */
static const GwyRGBA opengl_default_ambient  = { 0.2, 0.2, 0.2, 1.0 };
static const GwyRGBA opengl_default_diffuse  = { 0.8, 0.8, 0.8, 1.0 };
static const GwyRGBA opengl_default_specular = { 0.0, 0.0, 0.0, 1.0 };
static const GwyRGBA opengl_default_emission = { 0.0, 0.0, 0.0, 1.0 };
static const gdouble opengl_default_shininess = 0.0;

G_DEFINE_TYPE(GwyGLMaterial, gwy_gl_material, GWY_TYPE_RESOURCE)

static void
gwy_gl_material_class_init(GwyGLMaterialClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    parent_class = GWY_RESOURCE_CLASS(gwy_gl_material_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->item_type.copy = gwy_gl_material_copy;

    res_class->name = "glmaterials";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_set_default_item_name(res_class->inventory,
                                        GWY_GL_MATERIAL_DEFAULT);
    res_class->dump = gwy_gl_material_dump;
    res_class->parse = gwy_gl_material_parse;
}

static void
gwy_gl_material_init(GwyGLMaterial *gl_material)
{
    gwy_debug_objects_creation(G_OBJECT(gl_material));

    gl_material->ambient = opengl_default_ambient;
    gl_material->diffuse = opengl_default_diffuse;
    gl_material->specular = opengl_default_specular;
    gl_material->emission = opengl_default_emission;
    gl_material->shininess = opengl_default_shininess;
}

/**
 * gwy_gl_material_fix_rgba:
 * @color: A color.
 *
 * Fixes color components to range -1..1.
 *
 * Returns: The fixed color.
 **/
static inline GwyRGBA
gwy_gl_material_fix_rgba(const GwyRGBA *color)
{
    GwyRGBA rgba;

    rgba.r = CLAMP(color->r, -1.0, 1.0);
    rgba.g = CLAMP(color->g, -1.0, 1.0);
    rgba.b = CLAMP(color->b, -1.0, 1.0);
    rgba.a = CLAMP(color->a, -1.0, 1.0);
    if (rgba.r != color->r
        || rgba.g != color->g
        || rgba.b != color->b
        || rgba.a != color->a)
        g_warning("Color component outside -1..1 range");

    return rgba;
}

/**
 * gwy_gl_material_get_ambient:
 * @gl_material: A GL material.
 *
 * Gets the ambient reflectance of a GL material.
 *
 * Returns: Ambient reflectance (owned by GL material, must not be modified
 *          nor freed).
 **/
const GwyRGBA*
gwy_gl_material_get_ambient(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->ambient;
}

/**
 * gwy_gl_material_set_ambient:
 * @gl_material: A GL material.
 * @ambient: Ambient reflectance.
 *
 * Sets the ambient reflectance of a GL material.
 **/
void
gwy_gl_material_set_ambient(GwyGLMaterial *gl_material,
                            const GwyRGBA *ambient)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->ambient = gwy_gl_material_fix_rgba(ambient);
    gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_diffuse:
 * @gl_material: A GL material.
 *
 * Gets the diffuse reflectance of a GL material.
 *
 * Returns: Diffuse reflectance (owned by GL material, must not be modified
 *          nor freed).
 **/
const GwyRGBA*
gwy_gl_material_get_diffuse(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->diffuse;
}

/**
 * gwy_gl_material_set_diffuse:
 * @gl_material: A GL material.
 * @diffuse: Diffuse reflectance.
 *
 * Sets the diffuse reflectance of a GL material.
 **/
void
gwy_gl_material_set_diffuse(GwyGLMaterial *gl_material,
                            const GwyRGBA *diffuse)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->diffuse = gwy_gl_material_fix_rgba(diffuse);
    gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_specular:
 * @gl_material: A GL material.
 *
 * Gets the specular reflectance of a GL material.
 *
 * Returns: Specular reflectance (owned by GL material, must not be modified
 *          nor freed).
 **/
const GwyRGBA*
gwy_gl_material_get_specular(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->specular;
}

/**
 * gwy_gl_material_set_specular:
 * @gl_material: A GL material.
 * @specular: Specular reflectance.
 *
 * Sets the specular reflectance of a GL material.
 **/
void
gwy_gl_material_set_specular(GwyGLMaterial *gl_material,
                             const GwyRGBA *specular)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->specular = gwy_gl_material_fix_rgba(specular);
    gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_emission:
 * @gl_material: A GL material.
 *
 * Gets the emission component of a GL material.
 *
 * Returns: Emission component (owned by GL material, must not be modified
 *          nor freed).
 **/
const GwyRGBA*
gwy_gl_material_get_emission(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->emission;
}

/**
 * gwy_gl_material_set_emission:
 * @gl_material: A GL material.
 * @emission: Emission component.
 *
 * Sets the emission component of a GL material.
 **/
void
gwy_gl_material_set_emission(GwyGLMaterial *gl_material,
                             const GwyRGBA *emission)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->emission = gwy_gl_material_fix_rgba(emission);
    gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_get_shininess:
 * @gl_material: A GL material.
 *
 * Gets the shininess value of a GL material.
 *
 * Returns: The shininess value (in range 0..1, not 0..128).
 **/
gdouble
gwy_gl_material_get_shininess(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), 0.0);
    return gl_material->shininess;
}

/**
 * gwy_gl_material_set_shininess:
 * @gl_material: A GL material.
 * @shininess: Shinniness value (in range 0..1, not 0..128).
 *
 * Sets the shininess value of a GL material.
 **/
void
gwy_gl_material_set_shininess(GwyGLMaterial *gl_material,
                              gdouble shininess)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->shininess = CLAMP(shininess, 0.0, 1.0);
    gwy_gl_material_changed(gl_material);
}

/**
 * gwy_gl_material_sample_to_pixbuf:
 * @gl_material: A GL material to sample.
 * @pixbuf: A pixbuf to sample gl_material to (in horizontal direction).
 *
 * Samples GL material to a provided pixbuf.
 **/
void
gwy_gl_material_sample_to_pixbuf(GwyGLMaterial *gl_material,
                                 GdkPixbuf *pixbuf)
{
    GwyGLMaterial *glm = gl_material;
    gint width, height, rowstride, i, j, bpp;
    gboolean has_alpha;
    guchar *row, *pdata;
    guchar alpha;
    gdouble p, q;

    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pdata = gdk_pixbuf_get_pixels(pixbuf);
    bpp = 3 + (has_alpha ? 1 : 0);

    q = (width == 1) ? 0.0 : 1.0/(width - 1.0);
    p = (height == 1) ? 0.0 : 1.0/(height - 1.0);
    alpha = (guchar)CLAMP(MAX_CVAL*glm->ambient.a, 0.0, 255.0);

    for (j = 0; j < width; j++) {
        gdouble VRp = j*q;
        gdouble s = pow(VRp, 128.0*glm->shininess);
        GwyRGBA s0;

        s0.r = glm->emission.r + 0.2*glm->ambient.r + glm->specular.r*s;
        s0.g = glm->emission.g + 0.2*glm->ambient.g + glm->specular.g*s;
        s0.b = glm->emission.b + 0.2*glm->ambient.b + glm->specular.b*s;

        for (i = 0; i < height; i++) {
            gdouble LNp = i*p;
            gdouble v;

            row = pdata + i*rowstride + j*bpp;

            v = s0.r + glm->diffuse.r*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            v = s0.g + glm->diffuse.g*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            v = s0.b + glm->diffuse.b*LNp;
            *(row++) = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

            if (has_alpha)
                *(row++) = alpha;
        }
    }
}

static void
gwy_gl_material_changed(GwyGLMaterial *gl_material)
{
    gwy_resource_data_changed(GWY_RESOURCE(gl_material));
}

void
_gwy_gl_material_class_setup_presets(void)
{
    static const GwyRGBA null_color = { 0, 0, 0, 0 };
    GwyResourceClass *klass;
    GwyGLMaterial *gl_material;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_GL_MATERIAL);

    /* Default */
    gl_material = gwy_gl_material_new(GWY_GL_MATERIAL_DEFAULT,
                                      NULL, NULL, NULL, NULL, 0.0,
                                      TRUE);
    gwy_inventory_insert_item(klass->inventory, gl_material);
    g_object_unref(gl_material);

    /* None */
    gl_material = gwy_gl_material_new(GWY_GL_MATERIAL_NONE,
                                      &null_color, &null_color,
                                      &null_color, &null_color, 0.0,
                                      TRUE);
    gwy_inventory_insert_item(klass->inventory, gl_material);
    g_object_unref(gl_material);

    /* The gl_material added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

static GwyGLMaterial*
gwy_gl_material_new(const gchar *name,
                    const GwyRGBA *ambient,
                    const GwyRGBA *diffuse,
                    const GwyRGBA *specular,
                    const GwyRGBA *emission,
                    gdouble shininess,
                    gboolean is_const)
{
    GwyGLMaterial *gl_material;

    g_return_val_if_fail(name, NULL);

    gl_material = g_object_new(GWY_TYPE_GL_MATERIAL,
                               "is-const", is_const,
                               NULL);
    if (ambient)
        gl_material->ambient = *ambient;
    if (diffuse)
        gl_material->diffuse = *diffuse;
    if (specular)
        gl_material->specular = *specular;
    if (emission)
        gl_material->emission = *emission;
    if (shininess >= 0)
        gl_material->shininess = shininess;

    g_string_assign(GWY_RESOURCE(gl_material)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(gl_material)->is_modified = !is_const;

    return gl_material;
}

gpointer
gwy_gl_material_copy(gpointer item)
{
    GwyGLMaterial *gl_material, *copy;

    g_return_val_if_fail(GWY_IS_GL_MATERIAL(item), NULL);

    gl_material = GWY_GL_MATERIAL(item);
    copy = gwy_gl_material_new(gwy_resource_get_name(GWY_RESOURCE(item)),
                               &gl_material->ambient,
                               &gl_material->diffuse,
                               &gl_material->specular,
                               &gl_material->emission,
                               gl_material->shininess,
                               FALSE);

    return copy;
}

static void
gwy_gl_material_dump_component(GString *str,
                               const GwyRGBA *rgba)
{
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    /* this is ugly.  I hate locales */
    g_ascii_formatd(buffer, sizeof(buffer), "%.6g", rgba->r);
    g_string_append(str, buffer);
    g_string_append_c(str, ' ');
    g_ascii_formatd(buffer, sizeof(buffer), "%.6g", rgba->g);
    g_string_append(str, buffer);
    g_string_append_c(str, ' ');
    g_ascii_formatd(buffer, sizeof(buffer), "%.6g", rgba->b);
    g_string_append(str, buffer);
    g_string_append_c(str, ' ');
    g_ascii_formatd(buffer, sizeof(buffer), "%g", rgba->a);
    g_string_append(str, buffer);
    g_string_append_c(str, '\n');
}

static void
gwy_gl_material_dump(GwyResource *resource,
                     GString *str)
{
    GwyGLMaterial *gl_material;
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

    g_return_if_fail(GWY_IS_GL_MATERIAL(resource));
    gl_material = GWY_GL_MATERIAL(resource);

    gwy_gl_material_dump_component(str, &gl_material->ambient);
    gwy_gl_material_dump_component(str, &gl_material->diffuse);
    gwy_gl_material_dump_component(str, &gl_material->specular);
    gwy_gl_material_dump_component(str, &gl_material->emission);

    g_ascii_formatd(buffer, sizeof(buffer), "%g", gl_material->shininess);
    g_string_append(str, buffer);
    g_string_append_c(str, '\n');
}

static gboolean
gwy_gl_material_parse_component(gchar *line,
                                GwyRGBA *rgba)
{
    gchar *end;

    rgba->r = g_ascii_strtod(line, &end);
    if (end == line)
        return FALSE;
    line = end;
    rgba->g = g_ascii_strtod(line, &end);
    if (end == line)
        return FALSE;
    line = end;
    rgba->b = g_ascii_strtod(line, &end);
    if (end == line)
        return FALSE;
    line = end;
    rgba->a = g_ascii_strtod(line, &end);
    if (end == line)
        return FALSE;

    return TRUE;
}

static GwyResource*
gwy_gl_material_parse(const gchar *text,
                      gboolean is_const)
{
    GwyGLMaterial *gl_material = NULL;
    GwyRGBA ambient, diffuse, specular, emission;
    gdouble shininess;
    GwyGLMaterialClass *klass;
    gchar *str, *p, *line, *end;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_GL_MATERIAL);
    g_return_val_if_fail(klass, NULL);

    p = str = g_strdup(text);

    if (!(line = gwy_str_next_line(&p))
        || !gwy_gl_material_parse_component(line, &ambient))
        goto fail;
    if (!(line = gwy_str_next_line(&p))
        || !gwy_gl_material_parse_component(line, &diffuse))
        goto fail;
    if (!(line = gwy_str_next_line(&p))
        || !gwy_gl_material_parse_component(line, &specular))
        goto fail;
    if (!(line = gwy_str_next_line(&p))
        || !gwy_gl_material_parse_component(line, &emission))
        goto fail;
    if (!(line = gwy_str_next_line(&p)))
        goto fail;
    shininess = g_ascii_strtod(line, &end);
    if (end == line) {
        line = NULL;
        goto fail;
    }

    gwy_gl_material_fix_rgba(&ambient);
    gwy_gl_material_fix_rgba(&diffuse);
    gwy_gl_material_fix_rgba(&specular);
    gwy_gl_material_fix_rgba(&emission);
    shininess = CLAMP(shininess, 0.0, 1.0);

    gl_material = gwy_gl_material_new("",
                                      &ambient, &diffuse, &specular, &emission,
                                      shininess,
                                      is_const);

fail:
    if (!line)
        g_warning("Cannot parse GL material.");
    g_free(str);
    return (GwyResource*)gl_material;
}

/**
 * gwy_gl_materials:
 *
 * Gets inventory with all the gl_materials.
 *
 * Returns: GLMaterial inventory.
 **/
GwyInventory*
gwy_gl_materials(void)
{
    return
        GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GL_MATERIAL))->inventory;
}

/************************** Documentation ****************************/

/**
 * GwyGLMaterial:
 *
 * The #GwyGLMaterial struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_gl_materials_get_gl_material:
 * @name: GLMaterial name.  May be %NULL to get default gl_material.
 *
 * Convenience macro to get a gl_material from gwy_gl_materials() by name.
 *
 * Returns: GLMaterial identified by @name or default gl_material if it does
 *          not exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
