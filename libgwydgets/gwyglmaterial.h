/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
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


#ifndef __GWY_GLMATERIAL_H__
#define __GWY_GLMATERIAL_H__

#include <GL/gl.h>

#include <glib-object.h>


G_BEGIN_DECLS

#define GWY_TYPE_GL_MATERIAL                (gwy_gl_material_get_type())
#define GWY_GL_MATERIAL(obj)  \
             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterial))
#define GWY_GL_MATERIAL_CLASS(klass) \
             (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GL_MATERIAL, GwyGLMaterial))
#define GWY_IS_GL_MATERIAL(obj) \
             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GL_MATERIAL))
#define GWY_IS_GL_MATERIAL_CLASS(klass) \
             (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GL_MATERIAL))
#define GWY_GL_MATERIAL_GET_CLASS(obj)\
             (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GL_MATERIAL, GwyGLMaterialClass))

#define GWY_GL_MATERIAL_NONE                 "None"
#define GWY_GL_MATERIAL_EMERALD              "Emerald"
#define GWY_GL_MATERIAL_JADE                 "Jade"
#define GWY_GL_MATERIAL_OBSIDIAN             "Obsidian"
#define GWY_GL_MATERIAL_PEARL                "Pearl"
#define GWY_GL_MATERIAL_RUBY                 "Ruby"
#define GWY_GL_MATERIAL_TURQUOISE            "Turquoise"
#define GWY_GL_MATERIAL_BRASS                "Brass"
#define GWY_GL_MATERIAL_BRONZE               "Bronze"
#define GWY_GL_MATERIAL_CHROME               "Chrome"
#define GWY_GL_MATERIAL_COPPER               "Copper"
#define GWY_GL_MATERIAL_GOLD                 "Gold"
#define GWY_GL_MATERIAL_SILVER               "Silver"


typedef struct _GwyGLMaterial      GwyGLMaterial;
typedef struct _GwyGLMaterialClass GwyGLMaterialClass;


struct _GwyGLMaterial
{
    GObject parent_instance;

    gchar *name;

    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat shininess;
} ;


struct _GwyGLMaterialClass {
    GObjectClass parent_class;

    GHashTable *materials;
};

typedef void (*GwyGLMaterialFunc)(const gchar *name,
                                  GwyGLMaterial *glmaterial,
                                  gpointer user_data);

GType             gwy_gl_material_get_type         (void) G_GNUC_CONST;
GObject*          gwy_gl_material_new              (const gchar *name);
G_CONST_RETURN
gchar*            gwy_gl_material_get_name         (GwyGLMaterial *gl_material);
gboolean          gwy_gl_material_set_name         (GwyGLMaterial *gl_material,
                                                    const gchar *name);
GwyGLMaterial*    gwy_gl_material_get_by_name      (const gchar *name);
gboolean          gwy_gl_material_exists           (const gchar *name);
void              gwy_gl_material_foreach          (GwyGLMaterialFunc callback,
                                                    gpointer user_data);
void              gwy_gl_material_setup_presets    (void);

G_END_DECLS


#endif /* glmaterial.h */
