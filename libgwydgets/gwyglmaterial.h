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

#define GWY_TYPE_GLMATERIAL                (gwy_glmaterial_get_type())
#define GWY_GLMATERIAL(obj)  \
             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GLMATERIAL, GwyGLMaterial))
#define GWY_GLMATERIAL_CLASS(klass) \
             (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GLMATERIAL, GwyGLMaterial))
#define GWY_IS_GLMATERIAL(obj) \
             (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GLMATERIAL))
#define GWY_IS_GLMATERIAL_CLASS(klass) \
             (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GLMATERIAL))
#define GWY_GLMATERIAL_GET_CLASS(obj)\
             (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GLMATERIAL, GwyGLMaterialClass))

#define GWY_GLMATERIAL_NONE                 "None"
#define GWY_GLMATERIAL_EMERALD              "Emerald"
#define GWY_GLMATERIAL_JADE                 "Jade"
#define GWY_GLMATERIAL_OBSIDIAN             "Obsidian"
#define GWY_GLMATERIAL_PEARL                "Pearl"
#define GWY_GLMATERIAL_RUBY                 "Ruby"
#define GWY_GLMATERIAL_TURQUOISE            "Turquoise"
#define GWY_GLMATERIAL_BRASS                "Brass"
#define GWY_GLMATERIAL_BRONZE               "Bronze"
#define GWY_GLMATERIAL_CHROME               "Chrome"
#define GWY_GLMATERIAL_COPPER               "Copper"
#define GWY_GLMATERIAL_GOLD                 "Gold"
#define GWY_GLMATERIAL_SILVER               "Silver"


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

GType             gwy_glmaterial_get_type         (void) G_GNUC_CONST;
GObject*          gwy_glmaterial_new              (const gchar *name);
G_CONST_RETURN
gchar*            gwy_glmaterial_get_name         (GwyGLMaterial *glmaterial);
gboolean          gwy_glmaterial_set_name         (GwyGLMaterial *glmaterial,
                                                       const gchar *name);
GwyGLMaterial*    gwy_glmaterial_get_by_name      (const gchar *name);
gboolean          gwy_glmaterial_exists           (const gchar *name);
void              gwy_glmaterial_foreach          (GwyGLMaterialFunc callback,
                                                       gpointer user_data);
void              gwy_glmaterial_setup_presets    (void);

G_END_DECLS


#endif /* glmaterial.h */
