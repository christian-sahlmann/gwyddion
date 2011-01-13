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

GwyDelaunayVertex *initPoints(gdouble *x, gdouble *y, gdouble *z, 
                   gdouble *u, gdouble *v, gdouble *w, gint n);

void     gwy_delaunay_interpolate3_3(gdouble  x, gdouble  y, gdouble  z, 
                        gdouble *u, gdouble *v, gdouble *w, GwyDelaunayMesh *m);
                        
#endif

