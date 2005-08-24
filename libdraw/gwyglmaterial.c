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

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static gpointer       gwy_gl_material_copy       (gpointer);
static void           gwy_gl_material_sample_real(GwyGLMaterial *gl_material,
                                                  gint nsamples,
                                                  guchar *samples);
static void           gwy_gl_material_changed    (GwyGLMaterial *gl_material);
static GwyGLMaterial* gwy_gl_material_new        (const gchar *name,
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
static const gdouble opengl_default_shininess = 1.0;

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
 * Fixes color components to range 0..1.
 *
 * Returns: The fixed color.
 **/
static inline GwyRGBA
gwy_gl_material_fix_rgba(const GwyRGBA *color)
{
    GwyRGBA rgba;

    rgba.r = CLAMP(color->r, 0.0, 1.0);
    rgba.g = CLAMP(color->g, 0.0, 1.0);
    rgba.b = CLAMP(color->b, 0.0, 1.0);
    rgba.a = CLAMP(color->a, 0.0, 1.0);
    if (rgba.r != color->r
        || rgba.g != color->g
        || rgba.b != color->b
        || rgba.a != color->a)
        g_warning("Color component outside 0..1 range");

    return rgba;
}

const GwyRGBA*
gwy_gl_material_get_ambient(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->ambient;
}

void
gwy_gl_material_set_ambient(GwyGLMaterial *gl_material,
                            const GwyRGBA *ambient)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->ambient = gwy_gl_material_fix_rgba(ambient);
    gwy_gl_material_changed(gl_material);
}

const GwyRGBA*
gwy_gl_material_get_diffuse(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->diffuse;
}

void
gwy_gl_material_set_diffuse(GwyGLMaterial *gl_material,
                            const GwyRGBA *diffuse)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->diffuse = gwy_gl_material_fix_rgba(diffuse);
    gwy_gl_material_changed(gl_material);
}

const GwyRGBA*
gwy_gl_material_get_specular(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->specular;
}

void
gwy_gl_material_set_specular(GwyGLMaterial *gl_material,
                             const GwyRGBA *specular)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->specular = gwy_gl_material_fix_rgba(specular);
    gwy_gl_material_changed(gl_material);
}

const GwyRGBA*
gwy_gl_material_get_emission(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), NULL);
    return &gl_material->emission;
}

void
gwy_gl_material_set_emission(GwyGLMaterial *gl_material,
                             const GwyRGBA *emission)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->emission = gwy_gl_material_fix_rgba(emission);
    gwy_gl_material_changed(gl_material);
}

gdouble
gwy_gl_material_get_shininess(GwyGLMaterial *gl_material)
{
    g_return_val_if_fail(GWY_IS_GL_MATERIAL(gl_material), 0.0);
    return gl_material->shininess;
}

void
gwy_gl_material_set_shininess(GwyGLMaterial *gl_material,
                              gdouble shininess)
{
    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    gl_material->shininess = shininess;
    gwy_gl_material_changed(gl_material);
}

static void
gwy_gl_material_sample_real(GwyGLMaterial *gl_material,
                            gint nsamples,
                            guchar *samples)
{
    gint i;
    guchar alpha;
    gdouble q;

    q = 1.0/(nsamples - 1.0);
    /* FIXME */
    if (gwy_strequal(gwy_resource_get_name(GWY_RESOURCE(gl_material)),
                     GWY_GL_MATERIAL_NONE))
        alpha = 0;
    else
        alpha = 255;

    for (i = 0; i < nsamples; i++) {
        gdouble NL = i*q;
        gdouble VR = 2.0*NL*NL - 1.0;
        gdouble s = VR > 0.0 ? exp(log(VR)*128.0*gl_material->shininess) : 0.0;
        gdouble v;

        v = gl_material->ambient.r
            + gl_material->diffuse.r*NL
            + gl_material->specular.r*s;
        samples[4*i + 0] = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

        v = gl_material->ambient.g
            + gl_material->diffuse.g*NL
            + gl_material->specular.g*s;
        samples[4*i + 0] = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

        v = gl_material->ambient.b
            + gl_material->diffuse.b*NL
            + gl_material->specular.b*s;
        samples[4*i + 0] = (guchar)CLAMP(MAX_CVAL*v, 0.0, 255.0);

        /* FIXME */
        samples[4*i + 3] = alpha;
    }
}

/**
 * gwy_gl_material_sample_to_pixbuf:
 * @gl_material: A GL material to sample.
 * @pixbuf: A pixbuf to sample gl_material to (in horizontal direction).
 *
 * Samples GL material to a provided pixbuf.
 *
 * Unlike gwy_gl_material_sample() which simply takes samples at equidistant
 * points this method uses supersampling and thus gives a bit better looking
 * GL Material presentation.
 **/
void
gwy_gl_material_sample_to_pixbuf(GwyGLMaterial *gl_material,
                                 GdkPixbuf *pixbuf)
{
    /* Supersample to capture abrupt changes and peaks more faithfully.
     * Note an even number would lead to biased integer averaging. */
    enum { SUPERSAMPLE = 3 };
    gint width, height, rowstride, i, j;
    gboolean has_alpha, must_free_data;
    guchar *data, *pdata;

    g_return_if_fail(GWY_IS_GL_MATERIAL(gl_material));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    pdata = gdk_pixbuf_get_pixels(pixbuf);

    /* Usually the pixbuf is large enough to be used as a scratch space,
     * there is no need to allocate extra memory then. */
    if ((must_free_data = (SUPERSAMPLE*width*4 > rowstride*height)))
        data = g_new(guchar, SUPERSAMPLE*width*4);
    else
        data = pdata;

    gwy_gl_material_sample_real(gl_material, SUPERSAMPLE*width, data);

    /* Scale down to original size */
    for (i = 0; i < width; i++) {
        guchar *row = data + 4*SUPERSAMPLE*i;
        guint r, g, b, a;

        r = g = b = a = SUPERSAMPLE/2;
        for (j = 0; j < SUPERSAMPLE; j++) {
            r += *(row++);
            g += *(row++);
            b += *(row++);
            a += *(row++);
        }
        *(pdata++) = r/SUPERSAMPLE;
        *(pdata++) = g/SUPERSAMPLE;
        *(pdata++) = b/SUPERSAMPLE;
        if (has_alpha)
            *(pdata++) = a/SUPERSAMPLE;
    }

    /* Duplicate rows */
    pdata = gdk_pixbuf_get_pixels(pixbuf);
    for (i = 1; i < height; i++)
        memcpy(pdata + i*rowstride, pdata, rowstride);

    if (must_free_data)
        g_free(data);
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
    gl_material = gwy_gl_material_new(GWY_GL_MATERIAL_DEFAULT, TRUE);
    gwy_inventory_insert_item(klass->inventory, gl_material);
    g_object_unref(gl_material);

    /* None */
    gl_material = gwy_gl_material_new(GWY_GL_MATERIAL_NONE, TRUE);
    gwy_gl_material_set_ambient(gl_material, &null_color);
    gwy_gl_material_set_diffuse(gl_material, &null_color);
    gwy_gl_material_set_specular(gl_material, &null_color);
    gwy_gl_material_set_emission(gl_material, &null_color);
    gwy_inventory_insert_item(klass->inventory, gl_material);
    g_object_unref(gl_material);

    /* The gl_material added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

static GwyGLMaterial*
gwy_gl_material_new(const gchar *name,
                    gboolean is_const)
{
    GwyGLMaterial *gl_material;

    g_return_val_if_fail(name, NULL);

    gl_material = g_object_new(GWY_TYPE_GL_MATERIAL,
                               "is-const", is_const,
                               NULL);
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
                               FALSE);
    copy->ambient = gl_material->ambient;
    copy->diffuse = gl_material->diffuse;
    copy->specular = gl_material->specular;
    copy->emission = gl_material->emission;
    copy->shininess = gl_material->shininess;

    return copy;
}

static void
gwy_gl_material_dump(GwyResource *resource,
                     GString *str)
{
    GwyGLMaterial *gl_material;
    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    guint i;

    g_return_if_fail(GWY_IS_GL_MATERIAL(resource));
    gl_material = GWY_GL_MATERIAL(resource);
}

static GwyResource*
gwy_gl_material_parse(const gchar *text,
                      gboolean is_const)
{
    GwyGLMaterial *gl_material = NULL;
    GwyGLMaterialClass *klass;
    gchar *str, *p, *line, *end;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_GL_MATERIAL);
    g_return_val_if_fail(klass, NULL);

    p = str = g_strdup(text);

fail:
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
    return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GL_MATERIAL))->inventory;
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
