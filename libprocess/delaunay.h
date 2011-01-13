/******************************************************************************/
/*

  delaunay.h - By Ross Hemsley Aug. 2009 - rh7223@bris.ac.uk.
  
  This module will compute the delaunay triangulation of a set of uniformly
  distributed points in R^3. We will use the iterative edge flipping
  algorithm to add points one at a time.
  
  To store the triangulation, we start by just storing simplicies with pointers
  to their respective coordinates. 
  
  To simplify insertion, we first create a super-simplex with contains our
  entire dataset, this means that we don't have to specify any special
  conditions for the creation of our original simplicies.

  To make our algorithm robust we use Jonathan Shewchuk's Arbitrary Precision 
  Floating-poing Arithmetic predicates[1]  


  [1]  Routines for Arbitrary Precision Floating-point Arithmetic               
       and Fast Robust Geometric Predicates. May 18, 1996.
       Jonathan Rigchard Shewchuk.      
       
*/
/******************************************************************************/
#ifndef delaunay_h
#define delaunay_h
#include <glib.h>
/******************************************************************************/

/* These macros make code more readable. They allow us to access  
   the indexed elements of verticies directly.                    */
#define X v[0]
#define Y v[1]
#define Z v[2]
#define U data[0]
#define V data[1]
#define W data[2]

/******************************************************************************/

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
  // This is the location of this point.
  gdouble v[3];
  
  // This is the point index in the point list.
  // We only store this so that it is convenient for using the
  // tet-GwyDelaunayMesh function in Matlab.
  // We can remove it for when the code is actually used.
  gint    index;

  // These are the values for our vector field at this location.
  gdouble data[3];
  
  // We use this for caching the voronoi volume of this point.
  // it will provide a good speed-up!
  gdouble voronoiVolume;
  
} GwyDelaunayVertex; //vertex

/*******************************************************************************
* This is how we represent a Voronoi Cell in memory.                          
*******************************************************************************/

typedef struct 
{
  // The number of points on this cell, and the amount
  // of memory allocated for points on this cell.
  gint n, nallocated;
  // The array of points on this cell.
  gdouble **points;

  // This defines the cell, it contains a list of faces, each one is
  // consistantly oriented relative to itself (so traversing the pionts gives 
  // the convex hull of the face). Each face is seperated by a NULL pointer.
  // No gaurentee is made about the consistancy of orientations between 
  // different faces.
  arrayList *verticies;
  
} voronoiCell;


/******************************************************************************/
/* This is how we store an individual simplex: 4 pointers to the coordinates. */
/* We should try storing this without pointers probably.                      */
/******************************************************************************/

typedef struct _simplex
{
  // The verticies of this simplex.
  GwyDelaunayVertex  *p[4];
  // The neighbouring simlpicies of this simplex.
  // These are ordered in accordance with our 'get face' routine: 
  // so that the i'th face is shared with the i'th neighbour.
  struct _simplex *s[4];
  // This is the node in our auxillary list structure that holds this simplex.
  // It's far from an elegant solution: but it is fast and space-efficient.
  listNode *node;
} simplex;

/******************************************************************************/
/* We want to efficiently change back the neighbour pointers when             */
/* we remove a point.                                                         */
/******************************************************************************/

typedef struct
{
  stack  *ptrs;
  stack  *old;
} neighbourUpdate;

/******************************************************************************/
// We will keep all details of the GwyDelaunayMesh in this structure: thus hiding
// the complexities of memory pooling from the user. We also want to store
// the lists of most recently destroyed and allocated simplicies/neighbour
// updates, so that we speed up the 'remove last point' operation,
// which is _crucial_ to fast natural neighbour ginterpolation.
/******************************************************************************/


typedef struct
{
  // a linked list of all the simplicies.
  linkedList    *tets;
  
  // The simplex which contains all of the points.
  // its verticies contain no data values.
  simplex *super;
  GwyDelaunayVertex   superVerticies[4];
  
  // Memory pool.
  stack   *deadSimplicies;
  stack   *deadVoronoiCells;

  // We modify these when a point is inserted/removed.
  arrayList       *conflicts;
  arrayList       *updates;
  neighbourUpdate *neighbourUpdates;
  
  // Keep count of the number of degenerecies we find in the GwyDelaunayMesh, 
  // so that we can spot errors, and be aware of particularly degenerate data.
  gint coplanar_degenerecies;
  gint cospherical_degenerecies;

} GwyDelaunayMesh;


/******************************************************************************/

GwyDelaunayMesh* gwy_delaunay_new_mesh();
void             gwy_delaunay_free_GwyDelaunayMesh(GwyDelaunayMesh *m);
void             gwy_delaunay_remove_point(GwyDelaunayMesh *m);
void             gwy_delaunay_build_mesh(GwyDelaunayVertex* ps, gint n, GwyDelaunayMesh *m);
void             gwy_delaunay_add_point(GwyDelaunayVertex *p, GwyDelaunayMesh *m);
gint             gwy_delaunay_point_on_simplex(GwyDelaunayVertex *p, simplex *s);
voronoiCell*     gwy_delaunay_get_voronoi_cell(GwyDelaunayVertex *point, simplex *s0, GwyDelaunayMesh *m);
void             gwy_delaunay_vertex_by_scalar(gdouble *a, gdouble b, gdouble *out);
gdouble          gwy_delaunay_voronoi_cell_volume(voronoiCell *vc, GwyDelaunayVertex *p);
void             gwy_delaunay_free_voronoi_cell(voronoiCell *vc, GwyDelaunayMesh *m);
simplex*         gwy_delaunay_find_any_neighbour(GwyDelaunayVertex *v, arrayList *tets);
#endif

