/*******************************************************************************
*
*        delaunay.c - By Ross Hemsley Aug. 2009 - rh7223@bris.ac.uk.
*
* This file implements Delaunay meshing in 3D, using the edge flipping
* algorithm. To stop degenerecies arising from floating point errors, we use
* the geometical predicates provided in predicates.c - giving adaptive 
* floating point arithmetic. We also remove degenerecies present in data 
* caused by points which are coplanar, or cospherical. These points are removed
* by gradually adding random peterbations until the degenerecies are removed.
*
* This file has unit testing, which can be done by defining _TEST_ as shown
* seen below. The file can then be compiled by running:
*
*   >gcc -O3 delaunay.c utils.c
*
* The executible created can be run to create a set of random points, which are
* then meshed and checked for Delaunayness.
* 
*******************************************************************************/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "delaunay.h"
#include "predicates.c"
#include "assert.h"
#include <time.h>
#include "utils.c"


/* These macros make code more readable. They allow us to access  
   the indexed elements of verticies directly.                    */

static void             getRange(GwyDelaunayVertex *ps, gint n, GwyDelaunayVertex *min,
                                             GwyDelaunayVertex *max, GwyDelaunayVertex *range, gint r);
static void             initSuperSimplex(GwyDelaunayVertex *ps, gint n, GwyDelaunayMesh *m);
static gint              simplexContainsPoint(simplex *s, GwyDelaunayVertex *p);
static void             getFaceVerticies(simplex *s, gint i, GwyDelaunayVertex **p1, GwyDelaunayVertex **p2, 
                                                     GwyDelaunayVertex **p3, GwyDelaunayVertex **p4 );
static gint              vercmp(GwyDelaunayVertex *v1, GwyDelaunayVertex *v2);
static void             addSimplexToMesh(GwyDelaunayMesh *m, simplex *s);
static void             removeSimplexFromMesh(GwyDelaunayMesh *m, simplex *s);
static simplex*         findContainingSimplex(GwyDelaunayMesh *m, GwyDelaunayVertex *p);
static gint              isDelaunay(simplex *s, GwyDelaunayVertex *p);
static simplex**        swapSimplexNeighbour(simplex *s, simplex *old, simplex *new);
static simplex*         findNeighbour(simplex *s, GwyDelaunayVertex *p);
static gint              isConvex(GwyDelaunayVertex *v1, GwyDelaunayVertex *v2, GwyDelaunayVertex *v3, 
                                      GwyDelaunayVertex *t,  GwyDelaunayVertex *b);
static arrayList*       findNeighbours(GwyDelaunayVertex *v, simplex *s);
static simplex*         newSimplex(GwyDelaunayMesh *m);
static void             circumCenter(simplex *s, gdouble *out);
static void             setNeighbours(arrayList *newTets);
static gint              shareThreePoints(simplex *s0, gint i, simplex *s1);
static void             vertexAdd(gdouble *a, gdouble *b, gdouble *out);
static void             vertexSub(gdouble *a, gdouble *b, gdouble *out);
static void             crossProduct(gdouble *b, gdouble *c, gdouble *out);
static gdouble           squaredDistance(gdouble *a);
static gdouble           scalarProduct(gdouble *a, gdouble *b);
static gdouble           volumeOfTetrahedron(gdouble *a,gdouble *b, gdouble *c, gdouble *d);
static void             removeExternalSimplicies(GwyDelaunayMesh *m);
static arrayList*       naturalNeighbours(GwyDelaunayVertex *v, GwyDelaunayMesh *m);
static void             writeVoronoiCellToFile(FILE* f, voronoiCell *vc);
static neighbourUpdate* initNeighbourUpdates();
static void             resetNeighbourUpdates(neighbourUpdate *nu);
static void             undoNeighbourUpdates(neighbourUpdate *nu);
static void             pushNeighbourUpdate(neighbourUpdate *nu, simplex **ptr,
                                                          simplex  *old);
static void             freeNeighbourUpdates(neighbourUpdate *nu);
static gint              getNumSimplicies(GwyDelaunayMesh *m);
static void             randomPerturbation(GwyDelaunayVertex *v, gint attempt);
static gint              numSphericalDegenerecies(GwyDelaunayMesh *m);
static gint              numPlanarDegenerecies(GwyDelaunayMesh *m);


/******************************************************************************/

/* Set this to be lower than the average distance between points. It is the
   amount that we will shift points by when we detect degenerecies. We
   gradually increase the value until the degenercy is removed                */
  #define PERTURBATION_VALUE  1e-9


#define MAX(x,y)  x<y ? y : x
#define MIN(x,y)  x>y ? y : x
#define SWAP(x,y)                                                              \
{                                                                              \
  gdouble tmp;                                                                  \
  tmp = x;                                                                     \
  x   = y;                                                                     \
  y   = tmp;                                                                   \
}

static simplex *newSimplex(GwyDelaunayMesh *m)
{
  simplex *s = pop(m->deadSimplicies);
 
  // Obviously, we aren't going to re-use the super simplex..
  if (s==m->super) s=0;
 
  if (!s)
  {
    s = g_malloc(sizeof(simplex));
  }
  s->s[0] = 0;
  s->s[1] = 0;
  s->s[2] = 0;
  s->s[3] = 0;
  
  return s;
}

/******************************************************************************/
// This will take a list of points, and a mesh struct, and create a 
// Delaunay Tetrahedralisation.

void gwy_delaunay_build_mesh(GwyDelaunayVertex* ps, gint n, GwyDelaunayMesh *m)
{
  gint i,j;

  // Seed the random function, we will use the random function to remove
  // any degenerecies as we find them.
  srand ( time(NULL) );
    
  // We have no degenerecies to start with.
  m->coplanar_degenerecies  = 0;
  m->cospherical_degenerecies = 0;

  // This simplex will contain our entire point-set.
  initSuperSimplex(ps, n, m);
  addSimplexToMesh(m, m->super);
  
  // Add each point to the mesh 1-by-1 using the Edge Flipping technique.
  for (i=0; i<n; i++)
  {
    gwy_delaunay_add_point(&ps[i], m);
    
    // Push conflicts to the memory pool.
    for (j=0; j<arrayListSize(m->conflicts); j++)
      push(m->deadSimplicies, getFromArrayList(m->conflicts, j));    
    
    // Reset the conflict and update lists.
    emptyArrayList(m->conflicts);
    emptyArrayList(m->updates);
    
    // Clear out the old neighobur update structs. (We don't use them here).
    resetNeighbourUpdates(m->neighbourUpdates);
    
    //printf("Meshing: %d%%.\n%c[1A", (gint)((i+1)/(gdouble)n *100),27);   
  }
}

/******************************************************************************/
// This will allow us to remove all the simplicies which are connected
// to the super simplex.
static void removeExternalSimplicies(GwyDelaunayMesh *m)
{
  listNode *iter = topOfLinkedList(m->tets);
  simplex *s;
 
  // Remove all simplicies which connect to the super simplex
  while ((s = nextElement(m->tets, &iter)))
  {
    if (  simplexContainsPoint(s, m->super->p[0]) || 
          simplexContainsPoint(s, m->super->p[1]) || 
          simplexContainsPoint(s, m->super->p[2]) ||
          simplexContainsPoint(s, m->super->p[3])     )      
    {     
      swapSimplexNeighbour(s->s[0], s, NULL);
      swapSimplexNeighbour(s->s[1], s, NULL);
      swapSimplexNeighbour(s->s[2], s, NULL);
      swapSimplexNeighbour(s->s[3], s, NULL);
      
      removeSimplexFromMesh(m, s);
    }
  }
}

/******************************************************************************/
// return the value that we modified.

static simplex** swapSimplexNeighbour(simplex *s, simplex *old, simplex *new)
{
  gint i,found=0;
  // If this neighbour is on the exterior, we don't need to do anything.
  if (!s) return NULL;
 
  // We are going to go through each of the elements children to see which one
  // points to the old simplex. When we find that value, we are going to swap 
  // it for the new simplex value.
  for (i=0;i<4;i++)
  {
    if (s->s[i] == old) 
    {
      found=1;
      break;    
    }
  }

  if (found) //FIXME this was not here in original file
    s->s[i] = new;

  assert(found);
  return &s->s[i];
}

/******************************************************************************/
// we are going to go through every face of every simplex to see if the
// orientation is consistent.

static void orientationTest(linkedList *tets)
{
  gint i; 
  gdouble o; 
  listNode *iter = topOfLinkedList(tets);
  simplex  *s;
  
  while((s = nextElement(tets, &iter)))
  {
    GwyDelaunayVertex *p1, *p2, *p3, *p4;
    
    // Go through every face of this simplex
    for (i=0;i<4;i++)
    {
      getFaceVerticies(s, i, &p1, &p2, &p3, &p4);
      o =  orient3dfast(p1->v, p2->v, p3->v, p4->v);
      assert (o>0);
    }
  }
}

/******************************************************************************/

static gint delaunayTest(GwyDelaunayMesh *m, GwyDelaunayVertex *ps, gint n)
{
 
 listNode *iter = topOfLinkedList(m->tets);
 simplex  *s;
 
 gint isDel=0;
 gint notDel=0;
  
 while ((s = nextElement(m->tets, &iter)))
 {
    // we want to see if this simplex is delaunay  
    gint i, succes=1;
    for (i=0; i<n; i++)
    {
      // if this point is not on the simplex, then it should not be within 
      // the circumsphere of this given simplex.
      if (! gwy_delaunay_point_on_simplex(&ps[i], s))
      {
        gdouble inSph = inspherefast(s->p[0]->v,
                                    s->p[1]->v, 
                                    s->p[2]->v, 
                                    s->p[3]->v,
                                    ps[i].v);
        if (inSph >= 0)
        {
          notDel++;
          succes = 0;
          break;
        }
      }
    
    } 
    if (succes) isDel ++;  
  }
 
  return notDel == 0;                                        
}

/******************************************************************************/
// This function is purely to test whether the set of neighbours of each 
// simplex is correct - If it is not reliable, then the program behaviour will
// be undeterministic: potentially giving a very difficult bug. 
// We only need to run this test when the code is modified.

static void faceTest(GwyDelaunayMesh *m)
{
  gint j;
  simplex *neighbour;
  
  // Set our iterator to point to the top of the tet list.
  listNode *iter = topOfLinkedList(m->tets);
  // The pointre to the current simplex that we are considering.
  simplex  *s;
  
  // Go through every simplex in the list.
  while ((s = nextElement(m->tets, &iter)))
  {
    // Go through each neighbour of the simplex (this is equivilent
    // to going through every face of the simplex).
    for (j=0;j<4;j++)
    {
      GwyDelaunayVertex  *p1, *p2, *p3, *p4, *t1, *t2, *t3, *t4;; 
      
      // Get the verticies of the face we are considering.      
      getFaceVerticies(s, j, &p1, &p2, &p3, &p4);
            
      // This is the neighbour that should share the given verticies.
      neighbour = s->s[j];    
             
      // This could be an outer-face: in which case, there is no neighbour here.
      if (neighbour != NULL)
      {          
        gint x,found=0;
        //assert(!s->s[j]->dead);
        
        // Go through each neighbour and see if it points to us. 
        // if it does (which it should) check the points match.
        for (x=0; x<4; x++)
        {
          if (neighbour && neighbour->s[x] && neighbour->s[x] == s)
          {
            found = 1;
            
            // Get the verticies of the face that we share with the current
            // simplex.
            getFaceVerticies(neighbour, x, &t1, &t2, &t3, &t4);
            
            // We want to check that these two simplicies share their first 
            // three verticies. 
            getFaceVerticies(neighbour, x, &t1, &t2, &t3, &t4);
             
            assert (vercmp(t1,p1) || vercmp(t2, p1) || vercmp(t3,p1));
            assert (vercmp(t1,p2) || vercmp(t2, p2) || vercmp(t3,p2));
            assert (vercmp(t1,p3) || vercmp(t2, p3) || vercmp(t3,p3));                          
          }
        }
        // We have a pointer to a neighbour which does not point back to us.
        assert(found);
      }
    }
  }
}

/******************************************************************************/

static gint vercmp(GwyDelaunayVertex *v1, GwyDelaunayVertex *v2)
{
  gint i;
  for (i=0; i<3; i++)
    if ( v1->v[i] != v2->v[i] ) return 0; 
  return 1;
}

/******************************************************************************/
// This is a slightly optimised method to find the containing simplex
// of a point. We go through each simplex, check to see which faces, if any
// face the point we are looking for. The first one we find that does, we
// follow that neighbour. If all the faces are oriented so that the point is
// not in front of them, then we know that we have found the containing simplex.
// It is likely to be provably O(n^1/2).


static simplex* findContainingSimplex(GwyDelaunayMesh *m, GwyDelaunayVertex *p)
{
  // This will arbitrarily get the first simplex to consider.
  // ideally we want to start from the middle, but chosing a random 
  // simplex will give us good performance in general.
  
  listNode *iter = topOfLinkedList(m->tets);
  simplex  *s    = nextElement(m->tets,&iter); 
  GwyDelaunayVertex *v1, *v2, *v3, *v4;
  
  gint i;
  for (i=0; i<4; i++)
  {
    // get the orientation of this face.
    getFaceVerticies(s, i, &v1, &v2, &v3, &v4);
    
    if ((orient3dfast(v1->v, v2->v, v3->v, p->v) < 0) && s->s[i])
    {
      // Go to the next simplex, and start the loop again.
      s = s->s[i];
      i = -1;
    }
  }
    
  // All the orientation tests passed: the point lies within/on the simplex.
  return s;
}

/******************************************************************************/
// Return, as 3 arrays of gdouble, the verticies of the face i of this simplex.
// This function aims to help us ensure consistant orientation.
// The last value is that of the remaining vertex which is left over.

static void getFaceVerticies(simplex *s, gint i, GwyDelaunayVertex **p1, GwyDelaunayVertex **p2, 
                                         GwyDelaunayVertex **p3, GwyDelaunayVertex **p4  )
{
  switch (i)
  {
    case 0:
      *p1 = s->p[0];
      *p2 = s->p[1];
      *p3 = s->p[2];      
      *p4 = s->p[3];
      break;
    case 1:
      *p1 = s->p[3];
      *p2 = s->p[1];
      *p3 = s->p[0];      
      *p4 = s->p[2];   
      break;
    case 2:
      *p1 = s->p[0];
      *p2 = s->p[2];
      *p3 = s->p[3];      
      *p4 = s->p[1];  
      break;
    case 3:  
      *p1 = s->p[3];
      *p2 = s->p[2];
      *p3 = s->p[1];      
      *p4 = s->p[0];  
      break;
  } 
}

/******************************************************************************/
// This routine will tell us whether or not a simplex contains a given point.
// To perform this test robustly, we will use the code provided by
// Jonathan Rigchard Shewchuk[3]. This code allows us to scale the precision 
// of our calculations so that we can be sure of valid results whilst retaining
// good performance in the general case.

static gint simplexContainsPoint(simplex *s, GwyDelaunayVertex *p)
{
  // To perform this test, we check the orientation of our point against 
  // the plane defined by each triangular face of our given simplex.
  // if the sign is always negative then the point lies within the simplex.
  
  gint i;
  
  // The points on this face.
  GwyDelaunayVertex *p1, *p2, *p3, *p4;  
  
  for (i=0; i<4; i++)
  {
    // Get the face values for this simplex.
    getFaceVerticies(s, i, &p1, &p2, &p3, &p4);
    if (orient3dfast(p1->v, p2->v, p3->v, p->v) < 0) return 0;
  }
  
  return 1;
}

/******************************************************************************/
// Write out all the tets in the list, except for those ones connected to 
// the points on S0: which we can use as the super simplex.

static void writeTetsToFile(GwyDelaunayMesh *m)
{
  simplex  *s;
  gint i, super;
  listNode *iter;

  FILE *f = fopen("./tets.mat", "wt");
  if (!f)
  {
    fprintf(stderr, "Could not open tet. file for writing.\n");
    exit(1);
  } 
  
  iter = topOfLinkedList(m->tets);
  while ((s = nextElement(m->tets, &iter)))
  {
    super = 0;
    for (i=0; i<4; i++)
      if (gwy_delaunay_point_on_simplex(s->p[i],m->super)) super =1;

    if (!super) 
      fprintf(f,"%d %d %d %d\n", s->p[0]->index, s->p[1]->index, 
                                 s->p[2]->index, s->p[3]->index);
  }
  fclose(f);  
}

/******************************************************************************/
// Add gradually larger random perturbations to this point, until we can
// get a sphere which is not degenerate.

static void randomPerturbation(GwyDelaunayVertex *v, gint attempt)
{
  gint i;
  for (i=0;i<3;i++)
  {
    // Get a [0,1] distributed random variable.
    gdouble rand01 = (gdouble)rand()/((gdouble)RAND_MAX + 1);
    // add a random perturbation to each component of this vertex.
    gdouble p = (rand01-0.5) * PERTURBATION_VALUE * (attempt+1);
    v->v[i] +=  p;
  }
}

/******************************************************************************/
// This routine will return 0 if the simplex is no longer Delaunay with 
// the addition of this new point, 1 if this simplex is still Delaunay
// with the addition of this new point, and -1 if this simplex is 
// degenerate with the addition of this new point (i.e. if the simplex is
// co-spherical.)

static gint isDelaunay(simplex *s, GwyDelaunayVertex *p)
{
  gdouble inSph;
 
  // If the orientation is incorrect, then the output will be indeterministic.
 // #if DEBUG >= 0
  gdouble orientation = orient3dfast(s->p[0]->v, 
                                    s->p[1]->v, 
                                    s->p[2]->v, 
                                    s->p[3]->v);


  if (orientation <= 0)
  {
    printf("orientation error: %p, %lf\n",s,orientation);

    exit(1);
  }
//  assert(orientation != 0);
//  assert(orientation >  0);

  //#endif
  inSph = inspherefast(  s->p[0]->v,
                                s->p[1]->v, 
                                s->p[2]->v, 
                                s->p[3]->v, p); 

            
  // We have a degenerecy.
  if (inSph == 0) return -1;
                
  return inSph < 0;

}

/******************************************************************************/
// We assume that the list is correct on starting (i.e. contains no
// non-conflicting simplicies).

static void updateConflictingSimplicies(GwyDelaunayVertex *p, GwyDelaunayMesh *m)
{
  gint i, isDel;
  // Get at least one simplex which contains this point.
  simplex *s0 = findContainingSimplex(m, p);
  simplex *current;
  
  // Go through each simplex, if it contains neighbours which are
  // not already present, which are not in the list already, 
  // and which are not delaunay, we add them to the list of conflicts
  stack *toCheck = newStack();
  push(toCheck, s0);

  while (!isEmpty(toCheck))
  {
    // pop the next one to check from the stack.
    current = pop(toCheck);
    
    isDel = isDelaunay(current,p); 
    
    // Check to see whether or not we have a degenerecy
    if (isDel == -1) 
    {     
      m->cospherical_degenerecies ++;
      i=0;
      while( isDel == -1 )
      {
        randomPerturbation(p,i);
        isDel = isDelaunay(current,p);
        //printf("Degenerecy removing for %p, attempt: %d\n",current,i);
        i++;
      }   
      
      // Start this function again now that we have moved the point.
      freeStack(toCheck,NULL);
      emptyArrayList(m->conflicts);
      updateConflictingSimplicies(p,m);
      return;
    }
    
    if ((!isDel) && (!arrayListContains(m->conflicts, current)))
    {
      // add this simplex, and check its neighbours.
      addToArrayList(m->conflicts, current);
      for (i=0; i<4;i++)
        if (current->s[i])
          push(toCheck, current->s[i]);
    }
  }
  freeStack(toCheck,NULL);
}

/******************************************************************************/
// Add a point by using the edge flipping algorithm.

void gwy_delaunay_add_point(GwyDelaunayVertex *p, GwyDelaunayMesh *m)
{
  gint i, j, k, attempt;
  gdouble o;
  simplex *new, *s, **update;

  // If the list arguments are NULL, then we create local lists.
  // Otherwise, we return the list of updates we did. 
  // This is so that we can easily perform point removal.

  // This will set a list of conflicting non-Delaunay simplicies in the mesh
  // structure.
  updateConflictingSimplicies(p,m);
  
  // We now have a list of simplicies which contain the point p within
  // their circum-sphere.
  // We now want to create a new tetrahedralisation in the polytope formed
  // by removing the old simplicies.
  // We know which faces we should connect to our point, by deleting every
  // face which is shared by another conflicting simplex.
  
  for (j=0; j< arrayListSize(m->conflicts); j++)
  {
     s = getFromArrayList(m->conflicts,j);
     
    // Now go through each face, if it is not shared by any other face
    // on the stack, we will create a new simplex which is joined to 
    // our point.
    for (i=0; i<4; i++)
    {
      GwyDelaunayVertex *v1, *v2, *v3, *v4;
      getFaceVerticies(s, i, &v1, &v2, &v3, &v4);
      
      // Now, check to see whether or not this face is shared with any 
      // other simplicies in the list.
      if (! arrayListContains(m->conflicts, s->s[i]))
      {
        // We will create a new simplex connecting this face to our point. 
        new = newSimplex(m);
        new->p[0] = v1;
        new->p[1] = v2;
        new->p[2] = v3;
        new->p[3] =  p;
        
        attempt = 0;
        // Detect degenerecies resulting from coplanar points.
        o = orient3dfast(v1->v, v2->v, v3->v, p->v);
        if (o<=0)
        {
          m->coplanar_degenerecies ++;
          while (o<=0)
          {
            randomPerturbation(p, attempt);
            o = orient3dfast(v1->v, v2->v, v3->v, p->v);
            attempt ++;
          }
          // We are going to have to start adding this point again.
          // That means removing all changes we have done so far.
          undoNeighbourUpdates(m->neighbourUpdates);
          for (k=0; k<arrayListSize(m->updates); k++)
          {
            removeSimplexFromMesh(m, getFromArrayList(m->updates,k));
            push(m->deadSimplicies, getFromArrayList(m->updates, k));    
          }
          emptyArrayList(m->updates);
          emptyArrayList(m->conflicts);
          // Start adding this point again, now that we have
          // (hopefully) removed the coplanar dependencies.
          gwy_delaunay_add_point(p,m);
          return;
        }
        
        // We know that every successful face will be pointing
        // outwards from the point. We can therefore directly set the neighbour
        // to be the same as the one that was with this face before.
        new->s[0] = s->s[i];
         
        // update, storing each neighbour pointer change we make.
        update = swapSimplexNeighbour(s->s[i], s, new);
        pushNeighbourUpdate(m->neighbourUpdates, update, s);

        // This is a list of all the new tets created whilst adding
        // this point.
        addToArrayList(m->updates, new);
        addSimplexToMesh(m, new);        
      }      
    }    
  }

  // Connect up the ginternal neighbours of all our new simplicies.
  setNeighbours(m->updates);  
  
  // Remove the conflicting simplicies.  
  for (i=0; i<arrayListSize(m->conflicts); i++)
  {
    s = getFromArrayList(m->conflicts, i);
    removeSimplexFromMesh(m,s);
  }
}

/******************************************************************************/
// Slightly quick and dirty way to connect up all the neighbours of the 
// new simplicies.

static void setNeighbours(arrayList *newTets)
{
  simplex *s, *s2;
  GwyDelaunayVertex *v1, *v2, *v3, *t1, *t2, *t3, *tmp;
  // Go through each new simplex.
  gint i, j, k;
  
  for (j=0; j<arrayListSize(newTets); j++)
  {
    s = getFromArrayList(newTets,j);

    // These are the verticies on the 2-simplex pointing outwards from 
    // the current point.   
    v1 = s->p[0];
    v2 = s->p[1];
    v3 = s->p[2];

    // We need to find neighbours for the edges (v1,v2) (v2,v3) (v3,v1)
    // We will do this by going through every other simplex in the list, 
    // and checking to see if its outward pointing face shares any of these
    // pairs. If it does, then we connect up the neighbours.
    
    for (k=0; k<arrayListSize(newTets); k++)
    {
      s2 = getFromArrayList(newTets,k); 
      if (s == s2) continue;
      // NOTE: we don't consider the outside face.
      // We want to know which side the neighbours are on.
      for (i=1; i<4; i++)
      {
        getFaceVerticies(s2, i, &t1, &t2, &t3, &tmp);
        // We now want to see if any of the edges (v1,v2) (v2,v3) (v3,v1) are 
        // on this triangle:
        if (      (v1 == t1 || v1 == t2 || v1 == t3) && 
                  (v2 == t1 || v2 == t2 || v2 == t3)    )
          s->s[1] = s2;        
        else if ( (v2 == t1 || v2 == t2 || v2 == t3) && 
                  (v3 == t1 || v3 == t2 || v3 == t3)    )
          s->s[3] = s2;
        else if ( (v3 == t1 || v3 == t2 || v3 == t3) && 
                  (v1 == t1 || v1 == t2 || v1 == t3)    )
          s->s[2] = s2;           
      }
    }
  }
}

/******************************************************************************/

static gint shareThreePoints(simplex *s0, gint i, simplex *s1)
{
  GwyDelaunayVertex *v1, *v2, *v3, *v4;
  
  getFaceVerticies(s0, i, &v1, &v2, &v3, &v4);

  return (gwy_delaunay_point_on_simplex(v1,s1) && gwy_delaunay_point_on_simplex(v2,s1) &&
          gwy_delaunay_point_on_simplex(v3,s1) );
}

/******************************************************************************/
// Prgint an edge of a simplex to an output stream.

static void printEdge(GwyDelaunayVertex *v1, GwyDelaunayVertex* v2, FILE *stream)
{
  fprintf(stream, "%lf %lf %lf   %lf %lf %lf\n",
                                   v1->X, v2->X, v1->Y, v2->Y, v1->Z, v2->Z);
}

/******************************************************************************/
// Does this simplex have the point p? - notice that we are comparing pointersa
// and not coordinates: so that duplicate coordinates will evaluate to not 
// equal.

gint gwy_delaunay_point_on_simplex(GwyDelaunayVertex *p, simplex *s)
{
  if (!s) return 0;
  
  if (p == s->p[0] || p == s->p[1] || p == s->p[2] || p == s->p[3])   
    return 1;

  return 0;
}

/******************************************************************************/
// This routine tell us the neighbour of a simplex which is _not_ connected
// to the given point. 

static simplex *findNeighbour(simplex *s, GwyDelaunayVertex *p)
{
  GwyDelaunayVertex *t1, *t2, *t3, *t4;
  gint i,found=0;
  for (i=0; i<4; i++)
  {
    getFaceVerticies(s, i, &t1, &t2, &t3, &t4);
    if (t4 == p) 
    {
      found = 1;
      break;
    }
  }
  
  // If this fails then we couldn't find this point on this simplex.
  assert(found);
  
  return s->s[i];
}

/******************************************************************************/
// Check to see if the two simplicies sharing face v1, v2, v3 with top point
// t, and bottom point b are convex: i.e. can we draw a line between t and b
// which passes through the 2-simplex v1,v2,v3.

static gint isConvex(GwyDelaunayVertex *v1, GwyDelaunayVertex *v2, GwyDelaunayVertex *v3, GwyDelaunayVertex *t, GwyDelaunayVertex *b)
{
  gint i=0;
  if (orient3dfast(v3->v, t->v, v1->v, b->v) < 0) i++; 
  if (orient3dfast(v1->v, t->v, v2->v, b->v) < 0) i++;  
  if (orient3dfast(v2->v, t->v, v3->v, b->v) < 0) i++;
  
  return (i==0);
}

/******************************************************************************/
// This will return an arrayList of verticies which are the Natural
// Neighbours of a point. This is currently not used, and is slow.

static arrayList *naturalNeighbours(GwyDelaunayVertex *v, GwyDelaunayMesh *m)
{
  simplex *s;
  // User is responsible for freeing this structure.
  arrayList *l = newArrayList();
  listNode  *iter = topOfLinkedList(m->tets);

  gint i;
  while ((s = nextElement(m->tets, &iter)))
    if (gwy_delaunay_point_on_simplex(v, s))
      for (i=0;i<4;i++)
        if ( (s->p[i] != v) && (! gwy_delaunay_point_on_simplex(s->p[i], m->super) )
                            && (! arrayListContains(l, s->p[i])) )
          addToArrayList(l, s->p[i]);

  return l;
}

/******************************************************************************/
// Given a point and a list of simplicies, we want to find any valid 
// neighbour of this point.

simplex *gwy_delaunay_find_any_neighbour(GwyDelaunayVertex *v, arrayList *tets)
{
  gint i;
  
  for (i=0; i<arrayListSize(tets); i++)
  {
    simplex *s = getFromArrayList(tets,i);
    if (gwy_delaunay_point_on_simplex(v, s)) return s; 
  }
  return NULL;
}

/******************************************************************************/
// This function will find the neighbours of a given point.
// given a simplex and at least one neighbour.
// This is much more efficient than the previous Natural Neighobour method, 
// because we take a local simplex and then check the neighbourhood for 
// matching simplicies.

static arrayList *findNeighbours(GwyDelaunayVertex *v, simplex *s)
{
  gint i;
  arrayList *l   = newArrayList(); 
  stack *toCheck = newStack();

  simplex *current;
  push(toCheck, s);

  while (!isEmpty(toCheck))
  {
    // pop the next one to check from the stack.
    current = pop(toCheck);
    
    // We try to chose the things most likely to fail first, to take 
    // advantage of lazy evaluation.
    if ( gwy_delaunay_point_on_simplex(v, current) && (! arrayListContains(l, current)) )
    {
      // add this simplex, and check its neighbours.
      addToArrayList(l, current);
      for (i=0; i<4;i++)
        if (current->s[i])
          push(toCheck, current->s[i]);
    }   
  }
  freeStack(toCheck,NULL);

  return l;   
}

/******************************************************************************/
// Given a simplex, we want to find the correctly oriented verticies which are
// not connected

static void getRemainingFace(simplex *s, GwyDelaunayVertex *p, GwyDelaunayVertex **v1, 
                                             GwyDelaunayVertex **v2, 
                                             GwyDelaunayVertex **v3  )
{
  gint i,found=0;
  GwyDelaunayVertex *tmp;
  for (i=0; i<4; i++)
  {
    getFaceVerticies(s, i, v1, v2, v3, &tmp);
    if (tmp == p) 
    {
      found = 1;
      break; 
    }
  }
  // Make sure that we found the point.
  assert(found);
}


/******************************************************************************/

static gint isNeighbour(simplex *s0, simplex *s1)
{
  gint i;
  for (i=0;i<4;i++)
    if (s0->s[i] == s1) return 1;
    
  return 0;
}

/******************************************************************************/

static voronoiCell *newVoronoiCell(GwyDelaunayMesh *m, gint n)
{

  gint i;
  voronoiCell *vc ;
  vc = pop(m->deadVoronoiCells);

  if (!vc)
  {
    vc = malloc(sizeof(voronoiCell));
    vc->verticies   = newArrayList();
    vc->nallocated  = 0;
    vc->points      = 0;
    #ifdef DEBUG
    VORONOI_MALLOC ++;  
    #endif
  } else {
    emptyArrayList(vc->verticies);
  }

  // Allocate memory for the point list.
  // We do a realloc, because we want to expand the array to the required size,
  // and then not have to do any more alloc's later. - This is basically
  // a memory pooling technique.
  if (n > vc->nallocated)
  {
    vc->points = realloc(vc->points, sizeof(gdouble)*n);
    
    for (i=vc->nallocated; i<n; i++)
    {
      #ifdef DEBUG
      VERTEX_MALLOC++;
      #endif
      vc->points[i] = malloc(sizeof(gdouble)*3);
    }
    vc->nallocated = n;
  }
  vc->n = n;
  return vc;
}

/******************************************************************************/

static void addVertexToVoronoiCell(voronoiCell *vc, gdouble *v)
{
  addToArrayList(vc->verticies, v); 
}

/******************************************************************************/
// We use a NULL pointer as a seperator between different faces.

static void startNewVoronoiFace(voronoiCell *vc)
{
  addToArrayList(vc->verticies, NULL);
}

/******************************************************************************/
// Given a list of conflicts from the last insert, a list of updates
// from the last insert, and the mesh. We can 'roll-back' the mesh to its
// previous state.

void gwy_delaunay_remove_point(GwyDelaunayMesh *m)
{
  gint i;
  simplex  *s;  

  for (i=0; i< arrayListSize(m->conflicts); i++)
  {
    s = getFromArrayList(m->conflicts,i);
    addSimplexToMesh(m,s);
  }
   
  undoNeighbourUpdates(m->neighbourUpdates);
  
  for (i=0; i<arrayListSize(m->updates); i++)
  {
    s = getFromArrayList(m->updates, i);
    removeSimplexFromMesh(m,s);
  }
}

/******************************************************************************/
// This will take a voronoi cell and calculate the volume.
// the point p is the point which the voronoi cell is defined about.

gdouble gwy_delaunay_voronoi_cell_volume(voronoiCell *vc, GwyDelaunayVertex *p)
{
  gint i,j;
  gdouble volume = 0;
  
  for (i=0; i<arrayListSize(vc->verticies); i++)
  {
    gdouble   *thisV;
    gdouble   *firstV;
    gdouble   *lastV = NULL;
    
    // Calculate the center point of this face.
    gdouble center[3] = {0,0,0};
    
    // Find the center point of this vertex.
    for (j=i; j<arrayListSize(vc->verticies); j++)
    {     
      thisV = getFromArrayList(vc->verticies, j);
           
      // We have reached the next face.
      if (!thisV) break;
      vertexAdd(thisV, center, center);          
    }

    // This will give us the center point of the face.
    gwy_delaunay_vertex_by_scalar(center, 1/(gdouble)(j-i), center);
       
    // First vertex on the face.
    firstV = getFromArrayList(vc->verticies, i);
    lastV  = NULL;
    
    for (j=i; j<arrayListSize(vc->verticies); j++)
    {
      // Get the current vertex from the face.
      thisV = getFromArrayList(vc->verticies,j);
      
      // We've reached the end of this face.
      if (thisV == NULL)
      {
        i=j;
        break;
      }
      // If we have two points to join up, add the volume.
      if (lastV)
        volume += volumeOfTetrahedron(thisV, lastV, p->v, center);   
      else 
        firstV = thisV;
      lastV = thisV;
    }
    // Add the first segment.
    volume += volumeOfTetrahedron(lastV, firstV, p->v, center);      
  }

  assert(volume>0);
  
  return volume;

}

/******************************************************************************/
// a different function for getting Voronoi Cells - actually slower than
// original, so has been removed. In theory does less computation, but has
// bigger overheads for dealing with memory etc.

static voronoiCell* getVoronoiCell2(GwyDelaunayVertex *point, simplex *s0, GwyDelaunayMesh *m)
{
  arrayList *neighbours = findNeighbours(point, s0);
  gint n                 = arrayListSize(neighbours);
  
  gint i,j,k, sIndex, vindex, thisIndex, *thisInt;
  GwyDelaunayVertex *thisVertex;
  arrayList *incidentSimplexLists;
  arrayList *incidentEdges;
  arrayList *thisList;
  simplex *s;
  gint currentIndex;
  simplex *currentSimplex, *firstSimplex, *lastConsidered, *thisSimplex;
   
  // Alloc the memory for our new cell.
  voronoiCell *vc = newVoronoiCell(m,n);
  
  // Set all the points to be used in this Voronoi cell. 
  // We do this by going through all n neighbouring simplicies, and calculating
  // their circum centers.
  for (i=0;i<n;i++)
    circumCenter((simplex*)getFromArrayList(neighbours,i), vc->points[i]);
  
  /* The following two lists must be updated atomically */
  
  // This is the list of incident edges.
  incidentEdges = newArrayList();
  
  // This is a list of lists of the simplicies attached to those incident edges.
  incidentSimplexLists = newArrayList();
  
  // This will extract every edge from the neighoburing simplicies.
  for (i=0; i<arrayListSize(neighbours); i++)
  {
    // get this simplex.
    sIndex = i;
    s = getFromArrayList(neighbours,i);

    // Go through every point on this simplex. We ignore one of these: which is
    // the point we are ginterpolating.
    for (j=0;j<4;j++)
    {
      // The vertex we are considering on the simplex.
      thisVertex = s->p[j];
      
      // If this is the point we are ginterpolating, ignore it.
      if (thisVertex == point) continue;
      
      vindex = arrayListGetIndex(incidentEdges, thisVertex);
      // This edge is already in the list of incident edges.
      if (vindex != -1)
      {
        // We the simplex incident to this edge to index'th list 
        // contained within the list incidentSimplicies 
        thisList = getFromArrayList(incidentSimplexLists, vindex);
        thisInt = malloc(sizeof(gint));
        *thisInt = sIndex;
        addToArrayList(thisList, thisInt);     
      // This edge has not yet been added.
      } else {

        // This edge has not been seen yet, create a new entry in our 
        // edge list, and create a list to contain the simplicies which 
        // are incident to it.
        thisList = newArrayList();
        thisInt = malloc(sizeof(gint));
        *thisInt = sIndex;
        addToArrayList(thisList, thisInt);
        // Note atomic adds: these must always be coherent!
        addToArrayList(incidentEdges, thisVertex);
        addToArrayList(incidentSimplexLists, thisList);
      }
    }
  }
  /* We now have a list of edges which are incident to the point
     being ginterpolated. (these are defined by one vertex only, as we 
     know that all edges are connected to one common point). For each of these
     entries in the edgeList, there is a list in the list incidentSimplexlists
     which contains a list of all simplicies which are incident to that edge. */

    for (i=0; i<arrayListSize(incidentEdges); i++)
    {
      // the current edge we are considering (defined by the edge which is not
      // the point being ginterpolated).
      arrayList *incidentSimplicies = getFromArrayList(incidentSimplexLists,i);
      
      // We now want to get the list of simplicies which are incidient to this
      // edge, with a consistant orientation, so that we can calculate
      // the volume.
      // To do this we are going to fetch one simplex at a time, see if it is
      // a neighbour to the simplex we last used. if it is, we make sure it is
      // not a neighbour that we already visited (because there are two
      // valid neighbours for each corresponding direction).
      
      currentIndex   = *(gint*)getFromArrayList(incidentSimplicies, 0);
      currentSimplex = getFromArrayList(neighbours, currentIndex);
      firstSimplex   = currentSimplex;
      lastConsidered = NULL;
      
      do
      {
        for (k=0; k<arrayListSize(incidentSimplicies); k++)
        {
          thisIndex   = *(gint*)getFromArrayList(incidentSimplicies,k);
          thisSimplex = getFromArrayList(neighbours, thisIndex);
          
          if (thisSimplex != lastConsidered && 
                                        isNeighbour(thisSimplex,currentSimplex))
          {

            addVertexToVoronoiCell(vc, vc->points[thisIndex]);
            
            lastConsidered = currentSimplex;  
            currentSimplex = thisSimplex;
            break;
          }
        }        
      } while(currentSimplex != firstSimplex);
     
      startNewVoronoiFace(vc);
    }
    
    
  // Free all of the lists.
  for (i=0; i<arrayListSize(incidentEdges); i++)
  {
    thisList = getFromArrayList(incidentSimplexLists, i);
    freeArrayList(thisList,free);
  }
  freeArrayList(incidentEdges, NULL);
  freeArrayList(incidentSimplexLists, NULL);  
  
  return vc;
}

/******************************************************************************/
// This will give us the volume of the voronoi cell about the point p.
// We pass a point, at least one simplex containing that point, and the mesh.

voronoiCell* gwy_delaunay_get_voronoi_cell(GwyDelaunayVertex *point, simplex *s0, GwyDelaunayMesh *m)
{
  simplex  *s;
  // Find the Natural Neighbour verticies of this point.
  arrayList *neighbours = findNeighbours(point, s0);
  gint n = arrayListSize(neighbours);
  simplex  *simps[n];
  voronoiCell *vc;
  gint i, j = 0, done[3*n];
  GwyDelaunayVertex      *edges[3*n];
  GwyDelaunayVertex *v1, *v2, *v3;
  gint first, current, lastConsidered;
  gint match;

  
  // If no neighbours were found, it could be because we are trying to 
  // get a cell outside of the points
  if (n==0)
  {
    if (!simplexContainsPoint(m->super, point))
      fprintf(stderr,"Error: point outside of delaunay triangulation. - "
                     "try extending the super-simplex and re-starting.\n");
    else
     fprintf(stderr, "Error: No neighbours found for point! - mesh appears "
                     "to be degenerate.\n");
    exit(1);
  }
  
  // Create a new voronoi cell.
  vc = newVoronoiCell(m,n);

  for (i=0; i<arrayListSize(neighbours); i++)
  {
    s = getFromArrayList(neighbours, i);
    getRemainingFace(s, point, &v1, &v2, &v3);
    
    // Add this simplex to the list note we add three points for each.
    simps[i]        = s;
    edges[3*i]      = v1;   
    edges[3*i+1]    = v2;
    edges[3*i+2]    = v3;
    
    done[3*i]       = 0;
    done[3*i+1]     = 0;
    done[3*i+2]     = 0;
 
    // Calculate the circumcenter of this simplex.
    circumCenter(s, vc->points[i]);
  }

  // For every edge that is in the list, we are going to get the first simplex
  // which is incident to it, and then draw a line from it to the next
  // neighbour that it is incident to it. This next neighbour will be chosen 
  // because it is NOT the last one we considered.  
  // We are effectively rotating around each edge which spans from the point
  // to one of its natural neighbours, and storing the circum-centers of 
  // the simplicies that we found.
  for (i=0; i<3*n; i++)
  {   
    // We don't want to recompute edges.
    if (done[i]) continue;
    
    // This is the current simplex.
    // We are going to find a neighbour for it, which shares an edge, 
    // and is NOT equal to lastConsidered.
    first   = i;
    current = i;
    lastConsidered = -1;
    // Create this voronoi face.

    do {
      match=0;
      for (j=0; j < 3*n; j++)
      {
        if (done[j]) continue;
       // if (done[j]) continue;
        // Is this edge shared?
        // Is this simplex a neighbour of the current simplex?
        // Are we making progress: is this a new neighbour?

        if ((edges[i] == edges[j]) && j != lastConsidered
                                   && isNeighbour(simps[current/3], simps[j/3]))
        {
          done[j] = 1;  
          match   = 1;      
          // Add this vertex to this face of this cell.
          addVertexToVoronoiCell(vc, vc->points[j/3]);
          lastConsidered = current;
          current = j;
          break;
        }      
      }  
    } while (match && (current != first));
    
    startNewVoronoiFace(vc);
  }
   
  freeArrayList(neighbours, NULL);

  return vc;
}

/******************************************************************************/

void gwy_delaunay_free_voronoi_cell(voronoiCell *vc, GwyDelaunayMesh *m)
{
  // We just push the cell to the memory pool.
  // We can free the memory pools manually, or let the program do it 
  // automatically at the end.
  push(m->deadVoronoiCells, vc);
}

/******************************************************************************/
// This writes a voronoi cell to a file. This uses (-1,-1,-1) as a special
// value to seperate faces, so: things will obviously break if there is a 
// circum center on this value... We only really use this function for testing
// so we allow this bug to become a "feature"!

static void writeVoronoiCellToFile(FILE* f, voronoiCell *vc)
{
  gint i;
  for (i=0; i<arrayListSize(vc->verticies); i++)
  {
    GwyDelaunayVertex *v = getFromArrayList(vc->verticies, i);
    
    if (!v)
      fprintf(f,"-1 -1 -1\n");
    
    else
      fprintf(f, "%lf %lf %lf\n", v->X, v->Y, v->Z);
  } 
}

/******************************************************************************/
// We should make sure that we only use these two functions for ginteracting
// with the global simplex list, otherwise the program will behave
// indeterministically.

static void addSimplexToMesh(GwyDelaunayMesh *m, simplex *s)
{
  s->node = addToLinkedList(m->tets, s);  
}

/******************************************************************************/

static void removeSimplexFromMesh(GwyDelaunayMesh *m, simplex *s)
{
  // The simplex has a special pointer which gives its location in the mesh
  // linked list. This allows us to easily remove it from the list.
  removeFromLinkedList(m->tets, s->node);
}

/******************************************************************************/
// This will create a 'super simplex' that contains all of our data to form a
// starting point for our triangulation.

static void initSuperSimplex(GwyDelaunayVertex *ps, gint n, GwyDelaunayMesh *m)
{
  gint i;
  
  // Get the range of our data set.
  GwyDelaunayVertex min,max,range;
  getRange(ps, n, &min, &max, &range,1);
  m->super = newSimplex(m);
  
 //  Make the super simplex bigger! TODO check this !
 // vertexByScalar(range.v, 4, range.v);

  //printf("range is %g %g %g    %g %g %g\n", min.X, min.Y, min.Z, max.X, max.Y, max.Z);  

  // We will go clockwise around the base, and then do the top.
  m->superVerticies[0].X =  min.X + range.X/2;
  m->superVerticies[0].Y =  max.Y + 3*range.Y; 
  m->superVerticies[0].Z =  min.Z - range.Z;


  m->superVerticies[1].X =  max.X + 2*range.X;
  m->superVerticies[1].Y =  min.Y - 2*range.Y;
  m->superVerticies[1].Z =  min.Z - range.Z;
  
  m->superVerticies[2].X =  min.X - 2*range.X;
  m->superVerticies[2].Y =  min.Y - 2*range.Y;
  m->superVerticies[2].Z =  min.Z - range.Z;

  m->superVerticies[3].X = min.X + range.X/2;
  m->superVerticies[3].Y = min.Y + range.Y/2;
  m->superVerticies[3].Z = max.Z + 2*range.Z;
  
  // The super-simplex doesn't have any neighbours.
  for (i=0;i<4;i++)
  {
    m->superVerticies[i].index = 0;
    m->super->p[i] = &m->superVerticies[i];
    m->super->s[i] = NULL;
  }
}

/******************************************************************************/
// We are using two stacks, instead of a struct, because it gives us good
// memory advantages. - We will always want about the same number of 
// neighbour updates: using two array-based stacks means that we can always have
// that memory allocated: we should not have to do any memory reallocation
// for the neighbour updating.
// We use a function, so that the process becomes atomic: we don't want 
// to end up with the two stacks being incoherent!

static void pushNeighbourUpdate(neighbourUpdate *nu, simplex **ptr, simplex *old)
{
  push(nu->ptrs, ptr);
  push(nu->old,  old);
}

/******************************************************************************/

static void freeNeighbourUpdates(neighbourUpdate *nu)
{
  freeStack(nu->ptrs, free);
  freeStack(nu->old,  free);
  free(nu);
}

/******************************************************************************/
// We will go through, and use our neighbour update list to change the 
// neighbour values back to their originals.

static void undoNeighbourUpdates(neighbourUpdate *nu)
{
  simplex **thisPtr;
  simplex  *thisSimplex;
  
  // We use isEmpty, because the pointers might sometimes be NULL.
  while (!isEmpty(nu->ptrs))
  {
    thisPtr     = pop(nu->ptrs);
    thisSimplex = pop(nu->old);  
    
    if (thisPtr)
      *thisPtr = thisSimplex;
  }
}

/******************************************************************************/

static void resetNeighbourUpdates(neighbourUpdate *nu)
{
  // This will empty the stacks, without freeing any memory. This is the key
  // to this 'memory-saving hack'.
  emptyStack(nu->ptrs);
  emptyStack(nu->old);
}

/******************************************************************************/

static neighbourUpdate *initNeighbourUpdates()
{
  neighbourUpdate *nu = malloc(sizeof(neighbourUpdate));
  nu->ptrs = newStack();
  nu->old  = newStack();
  return nu;
}

/******************************************************************************/
// Allocate all the strucutres required to magintain a mesh in memory.

GwyDelaunayMesh *gwy_delaunay_new_mesh()
{
  // Create the struct to hold all of the data strucutres.
  GwyDelaunayMesh *m             = g_malloc(sizeof(GwyDelaunayMesh));
  // Pointer to the super simplex.
  m->super            = NULL;
  // A linked list of simplicies: We can actually remove this without losing
  // any functionality (but it is useful for testing etc.).
  m->tets             = newLinkedList();
  // Instead of freeing old simplicies/voronoi cells, we put them on a stack
  // and reuse them as necesary.
  m->deadSimplicies   = newStack();
  m->deadVoronoiCells = newStack();
  // This is an array of currently conflicting simplicies.
  m->conflicts        = newArrayList();
  // This is an array of the most recently added simplicies.
  m->updates          = newArrayList();
  // This is an array describing the most recent neighbour updates performed.
  m->neighbourUpdates = initNeighbourUpdates();

  return m;
}

/******************************************************************************/

void gwy_delaunay_free_mesh(GwyDelaunayMesh *m)
{
  #ifdef DEBUG
  printf("Mallocs for vertex: %d.\n", VERTEX_MALLOC);
  printf("Mallocs for simplex: %d.\n", SIMPLEX_MALLOC);
  printf("Mallocs for voronoi: %d.\n", VORONOI_MALLOC);
  #endif
  
  free(m->super);
  freeStack(m->deadSimplicies,   free);
  
  while(!isEmpty(m->deadVoronoiCells))
  {
    voronoiCell *vc = pop(m->deadVoronoiCells);
    gint i;
    for (i=0;i<vc->nallocated; i++)
      free(vc->points[i]);
    free(vc->points);
    freeArrayList(vc->verticies, NULL);
    free(vc);
  }
  
  freeStack(m->deadVoronoiCells, NULL);
  freeLinkedList(m->tets,        free);
  freeArrayList(m->conflicts, free);
  freeArrayList(m->updates, NULL);
  freeNeighbourUpdates(m->neighbourUpdates);
  free(m); 
}

/******************************************************************************/
// This will give us the volume of the arbitrary tetrahedron formed by 
// v1, v2, v3, v4
// All arguments are arrays of length three of gdoubles.

static gdouble volumeOfTetrahedron(gdouble *a, gdouble *b, gdouble *c, gdouble *d)
{
  gdouble a_d[3], b_d[3], c_d[3], cross[3], v;
  
  vertexSub(a,d, a_d);
  vertexSub(b,d, b_d);
  vertexSub(c,d, c_d);
  
  crossProduct(b_d, c_d, cross);  
  v = scalarProduct(a_d, cross)/(gdouble)6;
   
  return (v >= 0) ? v : -v;
}

/******************************************************************************/

static gdouble squaredDistance(gdouble *a)
{
  return scalarProduct(a,a);
}

/******************************************************************************/
// Take the cross product of two verticies and put it in the vertex 'out'.
static void crossProduct(gdouble *b, gdouble *c, gdouble *out)
{
  out[0] = b[1] * c[2] - b[2] * c[1];
  out[1] = b[2] * c[0] - b[0] * c[2];
  out[2] = b[0] * c[1] - b[1] * c[0];  
}

/******************************************************************************/

static gdouble scalarProduct(gdouble *a, gdouble *b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/******************************************************************************/

static void vertexSub(gdouble *a, gdouble *b, gdouble *out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

/******************************************************************************/

static void vertexAdd(gdouble *a, gdouble *b, gdouble *out)
{
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
}

/******************************************************************************/
// Note that this modifies the actual value of the given vertex.

void gwy_delaunay_vertex_by_scalar(gdouble *a, gdouble b, gdouble *out)
{
  out[0] = a[0] * b;
  out[1] = a[1] * b;
  out[2] = a[2] * b;
}

/******************************************************************************/
// This function will compute the circumcenter of a given simplex.
// -it returns the radius.-

static void circumCenter(simplex *s, gdouble *out)
{
  GwyDelaunayVertex *a, *b, *c, *d;
 
  gdouble b_a[3]   , c_a[3]   , d_a[3], 
         cross1[3], cross2[3], cross3[3], 
         mult1[3] , mult2[3] , mult3[3], 
         sum[3];
  gdouble denominator;

  getFaceVerticies(s, 0, &a, &b, &c, &d);
 
  
  // Calculate diferences between points.
  vertexSub(b->v, a->v, b_a);
  vertexSub(c->v, a->v, c_a);
  vertexSub(d->v, a->v, d_a);
  
  // Calculate first cross product.
  crossProduct(b_a, c_a, cross1);
  
  // Calculate second cross product.
  crossProduct(d_a, b_a, cross2);
  
  // Calculate third cross product.
  crossProduct(c_a, d_a, cross3);

  gwy_delaunay_vertex_by_scalar(cross1, squaredDistance(d_a), mult1);
  gwy_delaunay_vertex_by_scalar(cross2, squaredDistance(c_a), mult2);
  gwy_delaunay_vertex_by_scalar(cross3, squaredDistance(b_a), mult3);
  
  // Add up the sum of the numerator.
  vertexAdd(mult1, mult2, sum);
  vertexAdd(mult3, sum  , sum);

  // Calculate the denominator.
  denominator = 2*scalarProduct(b_a, cross3);
  
  // Do the division, and output to out.
  gwy_delaunay_vertex_by_scalar(sum, 1/(gdouble)(denominator), out);
  
  vertexAdd(out, a->v, out);
  
  // Calculate the radius of this sphere. - We don't actually need this.
  // But if we need it for debugging, we can add it back in.
  // return sqrt((gdouble)squaredDistance(sum))/(gdouble)denominator;
}

/******************************************************************************/

static gint getNumSimplicies(GwyDelaunayMesh *m)
{
  return linkedListSize(m->tets);
}

/******************************************************************************/

static gint numSphericalDegenerecies(GwyDelaunayMesh *m)
{
  return m->cospherical_degenerecies;
}

/******************************************************************************/

static gint numPlanarDegenerecies(GwyDelaunayMesh *m)
{
  return m->coplanar_degenerecies;
}

/******************************************************************************/

static void getRange(GwyDelaunayVertex *ps, gint n, GwyDelaunayVertex *min, GwyDelaunayVertex *max, GwyDelaunayVertex *range, gint r)
{
  gint i;
  
  *min = ps[0];
  *max = ps[0];
  
  for (i=0; i<n; i++)
  {
    if (0)
    {
      ps[i].X +=  ((gdouble)rand() / ((gdouble)RAND_MAX + 1) -0.5);
      ps[i].Y +=  ((gdouble)rand() / ((gdouble)RAND_MAX + 1) -0.5);
      ps[i].Z +=  ((gdouble)rand() / ((gdouble)RAND_MAX + 1) -0.5);
    }
    
    max->X = MAX(max->X, ps[i].X);
    max->Y = MAX(max->Y, ps[i].Y);
    max->Z = MAX(max->Z, ps[i].Z);
    
    min->X = MIN(min->X, ps[i].X);
    min->Y = MIN(min->Y, ps[i].Y);
    min->Z = MIN(min->Z, ps[i].Z);   
  }
  
  for (i=0;i<3;i++)
    range->v[i] = max->v[i] - min->v[i];
}


