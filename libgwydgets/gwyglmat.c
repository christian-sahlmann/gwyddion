/* materialy opengl pro 3d widget
   (c) Martin Siler
*/

#include "gwyglmat.h"

static GwyGLMaterialProp mat_emerald = {
  {0.0215, 0.1745, 0.0215, 1.0},
  {0.07568, 0.61424, 0.07568, 1.0},
  {0.633, 0.727811, 0.633, 1.0},
  0.6
};

static GwyGLMaterialProp mat_jade = {
  {0.135, 0.2225, 0.1575, 1.0},
  {0.54, 0.89, 0.63, 1.0},
  {0.316228, 0.316228, 0.316228, 1.0},
  0.1
};

static GwyGLMaterialProp mat_obsidian = {
  {0.05375, 0.05, 0.06625, 1.0},
  {0.18275, 0.17, 0.22525, 1.0},
  {0.332741, 0.328634, 0.346435, 1.0},
  0.3
};

static GwyGLMaterialProp mat_pearl = {
  {0.25, 0.20725, 0.20725, 1.0},
  {1.0, 0.829, 0.829, 1.0},
  {0.296648, 0.296648, 0.296648, 1.0},
  0.088
};

static GwyGLMaterialProp mat_ruby = {
  {0.1745, 0.01175, 0.01175, 1.0},
  {0.61424, 0.04136, 0.04136, 1.0},
  {0.727811, 0.626959, 0.626959, 1.0},
  0.6
};

static GwyGLMaterialProp mat_turquoise = {
  {0.1, 0.18725, 0.1745, 1.0},
  {0.396, 0.74151, 0.69102, 1.0},
  {0.297254, 0.30829, 0.306678, 1.0},
  0.1
};

static GwyGLMaterialProp mat_brass = {
  {0.329412, 0.223529, 0.027451, 1.0},
  {0.780392, 0.568627, 0.113725, 1.0},
  {0.992157, 0.941176, 0.807843, 1.0},
  0.21794872
};

static GwyGLMaterialProp mat_bronze = {
  {0.2125, 0.1275, 0.054, 1.0},
  {0.714, 0.4284, 0.18144, 1.0},
  {0.393548, 0.271906, 0.166721, 1.0},
  0.2
};

static GwyGLMaterialProp mat_chrome = {
  {0.25, 0.25, 0.25, 1.0},
  {0.4, 0.4, 0.4, 1.0},
  {0.774597, 0.774597, 0.774597, 1.0},
  0.6
};

static GwyGLMaterialProp mat_copper = {
  {0.19125, 0.0735, 0.0225, 1.0},
  {0.7038, 0.27048, 0.0828, 1.0},
  {0.256777, 0.137622, 0.086014, 1.0},
  0.1
};

static GwyGLMaterialProp mat_gold = {
  {0.24725, 0.1995, 0.0745, 1.0},
  {0.75164, 0.60648, 0.22648, 1.0},
  {0.628281, 0.555802, 0.366065, 1.0},
  0.4
};

static GwyGLMaterialProp mat_silver = {
  {0.19225, 0.19225, 0.19225, 1.0},
  {0.50754, 0.50754, 0.50754, 1.0},
  {0.508273, 0.508273, 0.508273, 1.0},
  0.4
};

static GwyGLMaterialProp mat_none = {
  {0.0, 0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0, 0.0},
  0.0
};

GwyGLMaterialProp * gwyGL_mat_none = &mat_none;
GwyGLMaterials gwyGL_materials [] =
{
    {"None", &mat_none},
    {"Emarald", &mat_emerald},
    {"Jade",&mat_jade},
    {"Obsidian",&mat_obsidian},
    {"Pearl",&mat_pearl},
    {"Ruby",&mat_ruby},
    {"Turquoise",&mat_turquoise},
    {"Brass",&mat_brass},
    {"Bronze",&mat_bronze},
    {"Copper",&mat_copper},
    {"Gold",&mat_gold},
    {"Silver",&mat_silver},
    {"Chrome",&mat_chrome},
    {0, 0}
};
