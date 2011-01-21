/*******************************************************************************
*
*  interpolate.h - By Ross Hemsley Aug. 2009 - rh7223@bris.ac.uk.
*  
*  This unit will perform Natural-Neighbour interpolation. To do this, we first
*  need a Mesh of Delaunay Tetrahedrons for our input points. Each point to be
*  interpolated is then inserted into the mesh (remembering the steps that were
*  taken to insert it) and then the volume of the modified Voronoi cells 
*  (easily computed from the Delaunay Mesh) are used to weight the neighbouring
*  points. We can then revert the Delaunay mesh back to the original mesh by 
*  reversing the flips required to insert the point.
*
*******************************************************************************/

#ifndef natural_h
#define natural_h

#include <glib.h>

typedef struct
{
  gint top;
  gint slots;
  void** arr;
} stack;


/* This is how we store an array list. */
typedef struct
{
   gint   num_slots;
   gint   num_elements;
   void** arr;
} arrayList;

/* These structs are needed to store a (doubly) linked list. */
typedef struct _listNode
{
  void *data;
  struct _listNode *next;
  struct _listNode *prev;
} listNode;

typedef struct
{
  listNode *head;
  listNode *last;
  gint nelem;
  stack   *deadNodes;
} linkedList;

typedef struct
{
  gdouble v[3];
  gint    index;
  gdouble data[3];
  gdouble voronoiVolume;

} GwyDelaunayVertex;


typedef struct _simplex
{
  GwyDelaunayVertex  *p[4];
  struct _simplex *s[4];
  listNode *node;
} simplex;

/******************************************************************************/

typedef struct
{
  stack  *ptrs;
  stack  *old;
} neighbourUpdate;

typedef struct
{
  linkedList    *tets;

  simplex *super;
  GwyDelaunayVertex   superVerticies[4];

  stack   *deadSimplicies;
  stack   *deadVoronoiCells;

  arrayList       *conflicts;
  arrayList       *updates;
  neighbourUpdate *neighbourUpdates;

  gint coplanar_degenerecies;
  gint cospherical_degenerecies;

} GwyDelaunayMesh;

GwyDelaunayMesh* gwy_delaunay_new_mesh();
void             gwy_delaunay_build_mesh(GwyDelaunayVertex* ps, gint n, GwyDelaunayMesh *m);

GwyDelaunayVertex *initPoints(gdouble *x, gdouble *y, gdouble *z, 
                   gdouble *u, gdouble *v, gdouble *w, gint n);

void     gwy_delaunay_interpolate3_3(gdouble  x, gdouble  y, gdouble  z, 
                        gdouble *u, gdouble *v, gdouble *w, GwyDelaunayMesh *m);
                        
#endif

