#ifndef __GWY_GLMAT_H__
#define __GWY_GLMAT_H__

#include <GL/gl.h>

/* materialy opengl pro 3d widget
   (c) Martin Siler
*/

typedef struct _MaterialProp
{
  GLfloat ambient[4];
  GLfloat diffuse[4];
  GLfloat specular[4];
  GLfloat shininess;
} GwyGLMaterialProp;

typedef struct _GwyMaterialArray
{
   char * name;
   GwyGLMaterialProp * mat;
} GwyGLMaterials;

extern GwyGLMaterials      gwyGL_materials [];
extern GwyGLMaterialProp * gwyGL_mat_none;
#endif
