/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GL_MATERIAL_H__
#define __GWY_GL_MATERIAL_H__

#include <glib-object.h>
#include <libgwyddion/gwyresource.h>
#include <libdraw/gwyrgba.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GWY_GL_MATERIAL_DEFAULT "OpenGL-Default"
#define GWY_GL_MATERIAL_NONE    "None"

#define GWY_TYPE_GL_MATERIAL             (gwy_gl_material_get_type())
#define GWY_GL_MATERIAL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterial))
#define GWY_GL_MATERIAL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GL_MATERIAL, GwyGLMaterialClass))
#define GWY_IS_GL_MATERIAL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GL_MATERIAL))
#define GWY_IS_GL_MATERIAL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GL_MATERIAL))
#define GWY_GL_MATERIAL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterialClass))

typedef struct _GwyGLMaterial      GwyGLMaterial;
typedef struct _GwyGLMaterialClass GwyGLMaterialClass;

struct _GwyGLMaterial {
    GwyResource parent_instance;

    GwyRGBA ambient;
    GwyRGBA diffuse;
    GwyRGBA specular;
    GwyRGBA emission;
    GwyRGBA reserved;
    gdouble shininess;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGLMaterialClass {
    GwyResourceClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType          gwy_gl_material_get_type        (void) G_GNUC_CONST;
const GwyRGBA* gwy_gl_material_get_ambient     (GwyGLMaterial *gl_material);
void           gwy_gl_material_set_ambient     (GwyGLMaterial *gl_material,
                                                const GwyRGBA *ambient);
const GwyRGBA* gwy_gl_material_get_diffuse     (GwyGLMaterial *gl_material);
void           gwy_gl_material_set_diffuse     (GwyGLMaterial *gl_material,
                                                const GwyRGBA *diffuse);
const GwyRGBA* gwy_gl_material_get_specular    (GwyGLMaterial *gl_material);
void           gwy_gl_material_set_specular    (GwyGLMaterial *gl_material,
                                                const GwyRGBA *specular);
const GwyRGBA* gwy_gl_material_get_emission    (GwyGLMaterial *gl_material);
void           gwy_gl_material_set_emission    (GwyGLMaterial *gl_material,
                                                const GwyRGBA *emission);
gdouble        gwy_gl_material_get_shininess   (GwyGLMaterial *gl_material);
void           gwy_gl_material_set_shininess   (GwyGLMaterial *gl_material,
                                                gdouble shininess);
void           gwy_gl_material_sample_to_pixbuf(GwyGLMaterial *gl_material,
                                                GdkPixbuf *pixbuf);
void           gwy_gl_material_reset           (GwyGLMaterial *gl_material);

GwyInventory*  gwy_gl_materials                (void);
GwyGLMaterial* gwy_gl_materials_get_gl_material(const gchar *name);

G_END_DECLS

#endif /*__GWY_GL_MATERIAL_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
