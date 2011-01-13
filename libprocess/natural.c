/*******************************************************************************
*
*  interpolate.c - By Ross Hemsley Aug. 2009 - rh7223@bris.ac.uk.
*
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "delaunay.h"
#include "assert.h"
#include "natural.h"
#include "utils.c"

#define SQR(x)  (x)*(x)

/******************************************************************************/

GwyDelaunayVertex *initPoints(gdouble *x, gdouble *y, gdouble *z, 
                   gdouble *u, gdouble *v, gdouble *w, gint n)
{
  GwyDelaunayVertex* ps = g_malloc(sizeof(GwyDelaunayVertex) *n);

  gint i;

  for (i=0; i<n; i++)
  {
    ps[i].X = x[i];
    ps[i].Y = y[i];
    ps[i].Z = z[i];
    
    ps[i].U = u[i];
    ps[i].V = v[i];
    ps[i].W = w[i]; 
    
    ps[i].voronoiVolume = -1;   
    //printf("Loading: %d: %g %g %g %g %g %g\n", i, ps[i].X, ps[i].Y, ps[i].Z, ps[i].U, ps[i].V, ps[i].W);
  }
  
  return ps;
}


void lastNaturalNeighbours(GwyDelaunayVertex *v, GwyDelaunayMesh *m, arrayList *neighbours, 
                                               arrayList *neighbourSimplicies)
{
  gint i, j;
  simplex *this;

  for (i=0; i<arrayListSize(m->updates); i++)
  {
    this = getFromArrayList(m->updates,i); 
    for (j=0; j<4; j++)
    {     
      if (this->p[j] != v && (! arrayListContains(neighbours, this->p[j])) )
      {
        if ((! gwy_delaunay_point_on_simplex(this->p[j], m->super)))
        {
          addToArrayList(neighbours, this->p[j]);      
          addToArrayList(neighbourSimplicies, this);
        }
      }      
    }
  }
}

/******************************************************************************/

// This function will interpolate the value of a new vertex in a given 
// vector field.

void gwy_delaunay_interpolate3_3(gdouble  x, gdouble  y, gdouble  z, 
                     gdouble *u, gdouble *v, gdouble *w, GwyDelaunayMesh *m )
{
  gint i;
  
  arrayList *neighbours;
  arrayList *neighbourSimplicies;
  gdouble *neighbourVolumes;
  gdouble pointVolume;
  gdouble value[3] = {0,0,0};
  gdouble sum, weight;
  simplex *s;
  voronoiCell *pointCell;
  GwyDelaunayVertex *thisVertex;
  simplex *thisSimplex;
  voronoiCell *vc;    

  GwyDelaunayVertex p;
  p.X             =  x;
  p.Y             =  y;
  p.Z             =  z;
  p.index         = -1;
  p.voronoiVolume = -1;
  
  // Add the point to the Delaunay Mesh - storing the original state.
  gwy_delaunay_add_point(&p, m);    

  // Find the natural neighbours of the inserted point, and also keep 
  // a list of an arbitrary neighbouring simplex, this will give us faster
  // neighbour lookup later.
  neighbours          = newArrayList();  
  neighbourSimplicies = newArrayList();  
  lastNaturalNeighbours(&p, m, neighbours, neighbourSimplicies);

  // Calculate the volumes of the Voronoi Cells of the natural neighbours.
  neighbourVolumes = g_malloc(arrayListSize(neighbours) * sizeof(gdouble));

  // Calculate the 'before' volumes of each voronoi cell.
  for (i=0; i<arrayListSize(neighbours); i++)
  {
    thisVertex  = getFromArrayList(neighbours, i);
    thisSimplex = getFromArrayList(neighbourSimplicies,i);  
    vc      = gwy_delaunay_get_voronoi_cell(thisVertex, thisSimplex, m);    
    neighbourVolumes[i]  = gwy_delaunay_voronoi_cell_volume(vc, thisVertex);  
    gwy_delaunay_free_voronoi_cell(vc,m); 
  }

  // Calculate the volume of the new point's Voronoi Cell.
  // We just need any neighbour simplex to use as an entry point into the
  // mesh.
  s             = getFromArrayList(neighbourSimplicies,0);
  pointCell = gwy_delaunay_get_voronoi_cell(&p, s, m);
  pointVolume            = gwy_delaunay_voronoi_cell_volume(pointCell, &p);
  gwy_delaunay_free_voronoi_cell(pointCell,m);
         
  // Remove the last point.
  gwy_delaunay_remove_point(m);

  // Calculate the 'stolen' volume of each neighbouring Voronoi Cell,
  // by calculating the original volumes, and subtracting the volumes
  // given when the point was added.
  for (i=0; i<arrayListSize(neighbours); i++)
  {
    thisVertex   = getFromArrayList(neighbours, i);  
    
    // All verticies have -1 here to start with, so we can tell if 
    // we have already calculated this value, and use it again here.
    if (thisVertex->voronoiVolume < 0)
    {
      s           = gwy_delaunay_find_any_neighbour(thisVertex, m->conflicts);
      vc      = gwy_delaunay_get_voronoi_cell(thisVertex, s, m);
      thisVertex->voronoiVolume = gwy_delaunay_voronoi_cell_volume(vc, thisVertex);
      gwy_delaunay_free_voronoi_cell(vc,m);
    }
    neighbourVolumes[i]  = thisVertex->voronoiVolume-neighbourVolumes[i];
  }
   
  // Weight the data values of each natural neighbour using the volume
  // ratios.
  sum   = 0;

  for (i=0; i<arrayListSize(neighbours); i++)
  {
    thisVertex = getFromArrayList(neighbours, i);
    assert (neighbourVolumes[i]>= -0.001);
    
    // Get the weight of this vertex.
    weight = neighbourVolumes[i]/pointVolume;
    
    // Add this componenet to the result.
    sum      += weight;
    value[0] += weight * thisVertex->U;   
    value[1] += weight * thisVertex->V;   
    value[2] += weight * thisVertex->W;           
  }
  
  // Normalise the output.
  gwy_delaunay_vertex_by_scalar(value, (double)1/(double)sum, value);

  // If the sum is 0 or less, we will get meaningless output. 
  // If it is slightly greater than 1, this could be due to rounding errors.
  // We tolerate up to 0.1 here.  
  if (sum <= 0 || sum > 1.1)
  {
    fprintf(stderr, "Error: sum value: %lf, expected range (0,1].\n",sum);
    fprintf(stderr, "There could be a degenerecy in the mesh, either retry "
                    "(since input is randomised this may resolve the problem), "
                    "or try adding a random peterbation to every point.\n");
   // exit(1);
  }

  // Put the dead simplicies in the memory pool.
  for (i=0; i<arrayListSize(m->updates); i++)
    push(m->deadSimplicies, getFromArrayList(m->updates, i));

  // Free all the memory that we allocated whilst interpolating this point.
  emptyArrayList(m->conflicts);
  emptyArrayList(m->updates);
  
  // Free memory associated with adding this point.
  freeArrayList(neighbours,          NULL);
  freeArrayList(neighbourSimplicies, NULL); 
  free(neighbourVolumes);
  
  // set the output.
  *u = value[0];
  *v = value[1];
  *w = value[2];

}



