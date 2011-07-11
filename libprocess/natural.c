/*
 *  @(#) $Id: natural.c 11428 2010-10-18 08:34:36Z dn2010 $
 *  Copyright (C) 2009 Ross Hemsley, David Necas (Yeti), Petr Klapetek.
 *  E-mail: rh7223@bris.ac.uk, yeti@gwyddion.net, klapetek@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*******************************************************************************
* utils.h
*
* Written by Ross Hemsley for McStas. (September 2009)
*
* These are general purpose routines to be used anywhere they are needed.
* For specifics on use, and more implementation details, see utils.c
*******************************************************************************/
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include "natural.h"
#define SQR(x)  (x)*(x)


typedef struct _stack stack;
typedef struct _arrayList arrayList;
typedef struct _listNode listNode;
typedef struct _linkedList linkedList;
typedef struct _simplex simplex;
typedef struct _neighbourUpdate neighbourUpdate;


struct _stack
{
  gint top;
  gint slots;
  void** arr;
};


struct _arrayList
{
   gint   num_slots;
   gint   num_elements;
   void** arr;
};

struct _listNode
{
  void *data;
  struct _listNode *next;
  struct _listNode *prev;
};

struct _linkedList
{
  listNode *head;
  listNode *last;
  gint nelem;
  stack   *deadNodes;
};

struct _simplex
{
  GwyDelaunayVertex  *p[4];
  struct _simplex *s[4];
  listNode *node;
};


struct _neighbourUpdate
{
  stack  *ptrs;
  stack  *old;
};


struct _GwyDelaunayVertex
{
  gdouble v[3];
  gint    index;
  gdouble data[3];
  gdouble voronoiVolume;

};

struct _GwyDelaunayMesh
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

};





/*******************************************************************************
* Array list functions.
* *******************************************************************************/
static gint         addToArrayList(arrayList *l, void* element);
static void*       getFromArrayList (arrayList *l, gint index);
static gint         arrayListSize(arrayList *l);
static arrayList*  newArrayList();
static void        freeArrayList(arrayList *l, void (*destructor)(void *e));
static gint         arrayListContains(arrayList * l , void * element);
static void        emptyArrayList(arrayList *l);

/*******************************************************************************
 * * Doubly linked list functions.
 * *******************************************************************************/
static linkedList* newLinkedList();
static listNode*   addToLinkedList(linkedList *l, void *e);
static void*       nextElement(linkedList *l, listNode **last);
static listNode*   topOfLinkedList(linkedList *l);
static void        removeFromLinkedList(linkedList *l, listNode *ln);
static void        freeLinkedList(linkedList *l, void (*destructor)(void *e));

/*******************************************************************************
 * * Stack functions.
 * *******************************************************************************/
static stack*      newStack();
static void        push(stack *s, void*e);
static void*       pop(stack *s);
static void        freeStack(stack *s, void (*destructor)(void *e));
static gint         isEmpty(stack *s);
static void        emptyStack(stack *s);


/******************************************************************************
* utils.c
*
* Written by Ross Hemsley for McStas, September 2009.
* All code below is required for the gintperolation functions, though many of
* of the routines may be considered general enough for use anywhere they are
* required. Current utils include an array based list implementation, a doubly
* linked list implementation and an array based stack implementation.
*
*******************************************************************************/


/*******************************************************************************
* Array List implementation. We use this whenever we want arrays which can be
* resized dynamically. We expect O(1) amortised insert and extraction from this
* implementation. Other opertations are O(n).
*******************************************************************************/

static gint addToArrayList(arrayList *l, void* element)
{
   if (l->num_elements >= l->num_slots)
   {

      // we have to allocate more space
      l->num_slots *= 2;
      l->arr = realloc(l->arr, (l->num_slots*sizeof(void*)));
      // check that we haven't run out of memory
      if (l->arr == NULL)
      {
         //fprintf(stderr, "Error: Out of Memory.\n");
         return -1;
      }
   }
   // add the element
   l->arr[l->num_elements] = element;
   l->num_elements++;

   // return the index where this element can be found.
   return (l->num_elements -1);
}

/******************************************************************************/

static arrayList *newArrayList()
{
   arrayList *l;
   l = malloc(sizeof(arrayList));
   l->num_elements = 0;
   l->num_slots = 16;
   l->arr = malloc(16*sizeof(void*));
   return l;
}

/******************************************************************************/


// a special function, which only works when the arrayList contains only gint*
static gint arrayListContains(arrayList * l , void * e)
{
   gint i;
   for (i=0; i< l->num_elements; i++)
      if (e == l->arr[i]) return 1;
   return 0;
}

/******************************************************************************/

static gint arrayListSize(arrayList *l)
{
   return l->num_elements;
}

/******************************************************************************/

static void * getFromArrayList (arrayList *l, gint iindex)
{
   if(iindex >= 0 && iindex <  l->num_elements)
      return l->arr[iindex];

   return NULL;
}

/******************************************************************************/
// We keep the memory associated with this list, but set it's number of elements
// to zero. This is effectively the same function as a memory heap.

static void emptyArrayList(arrayList *l)
{
  l->num_elements=0;
}

/******************************************************************************/

static void freeArrayList(arrayList *l, void (*destructor)(void *e))
{
  gint i;

  if (destructor)
    for (i=0;i<arrayListSize(l); i++)
      destructor(getFromArrayList(l,i));

  free(l->arr);
  free(l);
}

/******************************************************************************
* Doubly-linked list implementation. We use this implementation of a list when
* we want to have faster extractions from very large lists. Using this
* implementation we expect O(1) from all operations, except accessing an element
* by index, which is O(n). In most cases this will be much slower than using
* the array list implementation. (Except fast pogint removal in large sets)
* because this implementation will not take advantage of the cache like the
* array list does.
*******************************************************************************/

static linkedList *newLinkedList()
{
  linkedList *l = malloc(sizeof(linkedList));

  l->deadNodes = newStack();
  l->head  = NULL;
  l->last  = NULL;
  l->nelem = 0;

  return l;
}

/******************************************************************************/

static listNode* addToLinkedList(linkedList *l, void *e)
{

  listNode *ln;

  if (!isEmpty(l->deadNodes))
    ln = pop(l->deadNodes);
  else
    ln = malloc(sizeof(listNode));

  ln->data = e;
  ln->next = NULL;
  ln->prev = l->last;

  // Put this element on the end. If this is the first element
  // then set the head.
  if (l->head)
    l->last->next = ln;
  else
    l->head = ln;
  l->last = ln;

  l->nelem ++;
  return ln;
}




/******************************************************************************/

static void *nextElement(G_GNUC_UNUSED linkedList *l, listNode **last)
{
  void *e;
  // If this is the end, return null.
  if (!*last) return NULL;
  // Move the iterator along to the next element,
  // and then return the data item
  e = (*last)->data;
  *last = (*last)->next;
  return e;
}


/******************************************************************************/

static listNode *topOfLinkedList(linkedList *l)
{
  return l->head;
}

/******************************************************************************/
// Note: this does not free the memory associated with the removed element.

static void removeFromLinkedList(linkedList *l, listNode *ln)
{
  if (!ln){
//    fprintf(stderr, "Error: Tried to remove null element from linkedList.\n");
    return;
  }
  // This could be the top of the linkedList: if it is: make sure we change the
  // head poginter to the new value.
  if (ln->prev)
    ln->prev->next = ln->next;
  else
    l->head = ln->next;

  // This could be the bottom of the linkedList: make sure we change the last poginter.
  // if it is.
  if (ln->next)
    ln->next->prev = ln->prev;
  else
    l->last = ln->prev;

  // Free the node, and update the element count.
  push(l->deadNodes, ln);
  l->nelem --;
}

/******************************************************************************/
// This will free the elements the linkedList, along with the elemnts
// using the given destructor.
static void freeLinkedList(linkedList *l, void (*destructor)(void *e))
{

  listNode *thisNode = l->head;
  while (thisNode)
  {
    listNode *tmp = thisNode->next;
    if (destructor) destructor(thisNode->data);
    free(thisNode);
    thisNode = tmp;
  }
  freeStack(l->deadNodes, free);
  free(l);
}

/*******************************************************************************
* This is a simple array-based implementation of a stack, implementing
* pop, push, isEmpty, size and empty.
* We use isEmpty to tell whether we can keep popping poginters (because we could
* pop a NULL poginter to the stack). We use the empty function to reset the
* stack to zero without popping any elements. This magintains the memory
* associated with the stack.
*******************************************************************************/

static stack *newStack()
{
  stack *s = malloc(sizeof(stack));

  s->top   = 0;
  s->slots = 2;
  s->arr = malloc(2*sizeof(void*));

  return s;
}


/******************************************************************************/
// When we are storing poginters which could be null, we need to have a check
// to see if the stack is empty.

static gint isEmpty(stack *s)
{
  return (s->top == 0);
}

/******************************************************************************/
// This will cause the stack to be empty: note, we have NOT freed any memory
// associated with the elements.

static void emptyStack(stack *s)
{
  s->top = 0;
}

/******************************************************************************/

static void  push(stack *s, void*e)
{
  if (s->top >= s->slots)
   {
      // we have to allocate more space
      s->slots *= 2;
      s->arr = realloc(s->arr, (s->slots*sizeof(void*)));
      // check that we haven't run out of memory
      if (s->arr == NULL)
      {
//         fprintf(stderr, "Error: Out of Memory.\n");
          return;
      }
   }
   // add the element
   s->arr[s->top] = e;
   s->top ++;
}

/******************************************************************************/

static void *pop(stack *s)
{
  // If the stack is empty
  if (s->top == 0) return NULL;
  s->top--;
  return s->arr[s->top];
}

/******************************************************************************/

static void freeStack(stack *s, void (*destructor)(void *e))
{
  void *e;
  if (destructor)
    while ((e = pop(s)))
      destructor(e);

  free(s->arr);
  free(s);
}

///////////////////end of utils.c ///////////////////////////////////////////////////

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

/* These macros make code more readable. They allow us to access
   the indexed elements of verticies directly.                    */
#define X v[0]
#define Y v[1]
#define Z v[2]
#define U data[0]
#define V data[1]
#define W data[2]

/******************************************************************************/

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

/******************************************************************************/

static void             gwy_delaunay_remove_point(GwyDelaunayMesh *m);
static void             gwy_delaunay_add_point(GwyDelaunayVertex *p, GwyDelaunayMesh *m);
static gint             gwy_delaunay_point_on_simplex(GwyDelaunayVertex *p, simplex *s);
static voronoiCell*     gwy_delaunay_get_voronoi_cell(GwyDelaunayVertex *point, simplex *s0, GwyDelaunayMesh *m);
static void             gwy_delaunay_vertex_by_scalar(gdouble *a, gdouble b, gdouble *out);
static gdouble          gwy_delaunay_voronoi_cell_volume(voronoiCell *vc, GwyDelaunayVertex *p);
static void             gwy_delaunay_free_voronoi_cell(voronoiCell *vc, GwyDelaunayMesh *m);
static simplex*         gwy_delaunay_find_any_neighbour(GwyDelaunayVertex *v, arrayList *tets);

/////////////////////////////////// end of delaunay.h //////////////////////////////////

//////////////////////////// predicates. c  /////////////////////////////////////////
/*****************************************************************************/
/*                                                                           */
/*  Routines for Arbitrary Precision Floating-point Arithmetic               */
/*  and Fast Robust Geometric Predicates                                     */
/*  (predicates.c)                                                           */
/*                                                                           */
/*  May 18, 1996                                                             */
/*                                                                           */
/*  Placed in the public domain by                                           */
/*  Jonathan Richard Shewchuk                                                */
/*  School of Computer Science                                               */
/*  Carnegie Mellon University                                               */
/*  5000 Forbes Avenue                                                       */
/*  Pittsburgh, Pennsylvania  15213-3891                                     */
/*  jrs@cs.cmu.edu                                                           */
/*                                                                           */
/*  This file contains C implementation of algorithms for exact addition     */
/*    and multiplication of floating-point numbers, and predicates for       */
/*    robustly performing the orientation and incircle tests used in         */
/*    computational geometry.  The algorithms and underlying theory are      */
/*    described in Jonathan Richard Shewchuk.  "Adaptive Precision Floating- */
/*    Point Arithmetic and Fast Robust Geometric Predicates."  Technical     */
/*    Report CMU-CS-96-140, School of Computer Science, Carnegie Mellon      */
/*    University, Pittsburgh, Pennsylvania, May 1996.  (Submitted to         */
/*    Discrete & Computational Geometry.)                                    */
/*                                                                           */
/*  This file, the paper listed above, and other information are available   */
/*    from the Web page http://www.cs.cmu.edu/~quake/robust.html .           */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Using this code:                                                         */
/*                                                                           */
/*  First, read the short or long version of the paper (from the Web page    */
/*    above).                                                                */
/*                                                                           */
/*  Be sure to call exactinit() once, before calling any of the arithmetic   */
/*    functions or geometric predicates.  Also be sure to turn on the        */
/*    optimizer when compiling this file.                                    */
/*                                                                           */
/*                                                                           */
/*  Several geometric predicates are defined.  Their parameters are all      */
/*    points.  Each point is an array of two or three floating-point         */
/*    numbers.  The geometric predicates, described in the papers, are       */
/*                                                                           */
/*    orient2d(pa, pb, pc)                                                   */
/*    orient2dfast(pa, pb, pc)                                               */
/*    orient3d(pa, pb, pc, pd)                                               */
/*    orient3dfast(pa, pb, pc, pd)                                           */
/*    incircle(pa, pb, pc, pd)                                               */
/*    incirclefast(pa, pb, pc, pd)                                           */
/*    insphere(pa, pb, pc, pd, pe)                                           */
/*    inspherefast(pa, pb, pc, pd, pe)                                       */
/*                                                                           */
/*  Those with suffix "fast" are approximate, non-robust versions.  Those    */
/*    without the suffix are adaptive precision, robust versions.  There     */
/*    are also versions with the suffices "exact" and "slow", which are      */
/*    non-adaptive, exact arithmetic versions, which I use only for timings  */
/*    in my arithmetic papers.                                               */
/*                                                                           */
/*                                                                           */
/*  An expansion is represented by an array of floating-point numbers,       */
/*    sorted from smallest to largest magnitude (possibly with interspersed  */
/*    zeros).  The length of each expansion is stored as a separate integer, */
/*    and each arithmetic function returns an integer which is the length    */
/*    of the expansion it created.                                           */
/*                                                                           */
/*  Several arithmetic functions are defined.  Their parameters are          */
/*                                                                           */
/*    e, f           Input expansions                                        */
/*    elen, flen     Lengths of input expansions (must be >= 1)              */
/*    h              Output expansion                                        */
/*    b              Input scalar                                            */
/*                                                                           */
/*  The arithmetic functions are                                             */
/*                                                                           */
/*    grow_expansion(elen, e, b, h)                                          */
/*    grow_expansion_zeroelim(elen, e, b, h)                                 */
/*    expansion_sum(elen, e, flen, f, h)                                     */
/*    expansion_sum_zeroelim1(elen, e, flen, f, h)                           */
/*    expansion_sum_zeroelim2(elen, e, flen, f, h)                           */
/*    fast_expansion_sum(elen, e, flen, f, h)                                */
/*    fast_expansion_sum_zeroelim(elen, e, flen, f, h)                       */
/*    linear_expansion_sum(elen, e, flen, f, h)                              */
/*    linear_expansion_sum_zeroelim(elen, e, flen, f, h)                     */
/*    scale_expansion(elen, e, b, h)                                         */
/*    scale_expansion_zeroelim(elen, e, b, h)                                */
/*    compress(elen, e, h)                                                   */
/*                                                                           */
/*  All of these are described in the long version of the paper; some are    */
/*    described in the short version.  All return an integer that is the     */
/*    length of h.  Those with suffix _zeroelim perform zero elimination,    */
/*    and are recommended over their counterparts.  The procedure            */
/*    fast_expansion_sum_zeroelim() (or linear_expansion_sum_zeroelim() on   */
/*    processors that do not use the round-to-even tiebreaking rule) is      */
/*    recommended over expansion_sum_zeroelim().  Each procedure has a       */
/*    little note next to it (in the code below) that tells you whether or   */
/*    not the output expansion may be the same array as one of the input     */
/*    expansions.                                                            */
/*                                                                           */
/*                                                                           */
/*  If you look around below, you'll also find macros for a bunch of         */
/*    simple unrolled arithmetic operations, and procedures for printing     */
/*    expansions (commented out because they don't work with all C           */
/*    compilers) and for generating random floating-point numbers whose      */
/*    significand bits are all random.  Most of the macros have undocumented */
/*    requirements that certain of their parameters should not be the same   */
/*    variable; for safety, better to make sure all the parameters are       */
/*    distinct variables.  Feel free to send email to jrs@cs.cmu.edu if you  */
/*    have questions.                                                        */
/*                                                                           */
/*****************************************************************************/


/* On some machines, the exact arithmetic routines might be defeated by the  */
/*   use of internal extended precision floating-point registers.  Sometimes */
/*   this problem can be fixed by defining certain values to be volatile,    */
/*   thus forcing them to be stored to memory and rounded off.  This isn't   */
/*   a great solution, though, as it slows the arithmetic down.              */
/*                                                                           */
/* To try this out, write "#define INEXACT volatile" below.  Normally,       */
/*   however, INEXACT should be defined to be nothing.  ("#define INEXACT".) */

#define INEXACT                          /* Nothing */
/* #define INEXACT volatile */

#define REAL double                      /* float or double */
#define REALPRINT doubleprint
#define REALRAND doublerand
#define NARROWRAND narrowdoublerand
#define UNIFORMRAND uniformdoublerand

/* Which of the following two methods of finding the absolute values is      */
/*   fastest is compiler-dependent.  A few compilers can inline and optimize */
/*   the fabs() call; but most will incur the overhead of a function call,   */
/*   which is disastrously slow.  A faster way on IEEE machines might be to  */
/*   mask the appropriate bit, but that's difficult to do in C.              */

#define Absolute(a)  ((a) >= 0.0 ? (a) : -(a))
/* #define Absolute(a)  fabs(a) */

/* Many of the operations are broken up into two pieces, a main part that    */
/*   performs an approximate operation, and a "tail" that computes the       */
/*   roundoff error of that operation.                                       */
/*                                                                           */
/* The operations Fast_Two_Sum(), Fast_Two_Diff(), Two_Sum(), Two_Diff(),    */
/*   Split(), and Two_Product() are all implemented as described in the      */
/*   reference.  Each of these macros requires certain variables to be       */
/*   defined in the calling routine.  The variables `bvirt', `c', `abig',    */
/*   `_i', `_j', `_k', `_l', `_m', and `_n' are declared `INEXACT' because   */
/*   they store the result of an operation that may incur roundoff error.    */
/*   The input parameter `x' (or the highest numbered `x_' parameter) must   */
/*   also be declared `INEXACT'.                                             */

#define Fast_Two_Sum_Tail(a, b, x, y) \
  bvirt = x - a; \
  y = b - bvirt

#define Fast_Two_Sum(a, b, x, y) \
  x = (REAL) (a + b); \
  Fast_Two_Sum_Tail(a, b, x, y)

#define Fast_Two_Diff_Tail(a, b, x, y) \
  bvirt = a - x; \
  y = bvirt - b

#define Fast_Two_Diff(a, b, x, y) \
  x = (REAL) (a - b); \
  Fast_Two_Diff_Tail(a, b, x, y)

#define Two_Sum_Tail(a, b, x, y) \
  bvirt = (REAL) (x - a); \
  avirt = x - bvirt; \
  bround = b - bvirt; \
  around = a - avirt; \
  y = around + bround

#define Two_Sum(a, b, x, y) \
  x = (REAL) (a + b); \
  Two_Sum_Tail(a, b, x, y)

#define Two_Diff_Tail(a, b, x, y) \
  bvirt = (REAL) (a - x); \
  avirt = x + bvirt; \
  bround = bvirt - b; \
  around = a - avirt; \
  y = around + bround

#define Two_Diff(a, b, x, y) \
  x = (REAL) (a - b); \
  Two_Diff_Tail(a, b, x, y)

#define Split(a, ahi, alo) \
  c = (REAL) (splitter * a); \
  abig = (REAL) (c - a); \
  ahi = c - abig; \
  alo = a - ahi

#define Two_Product_Tail(a, b, x, y) \
  Split(a, ahi, alo); \
  Split(b, bhi, blo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

#define Two_Product(a, b, x, y) \
  x = (REAL) (a * b); \
  Two_Product_Tail(a, b, x, y)

/* Two_Product_Presplit() is Two_Product() where one of the inputs has       */
/*   already been split.  Avoids redundant splitting.                        */

#define Two_Product_Presplit(a, b, bhi, blo, x, y) \
  x = (REAL) (a * b); \
  Split(a, ahi, alo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

/* Two_Product_2Presplit() is Two_Product() where both of the inputs have    */
/*   already been split.  Avoids redundant splitting.                        */

#define Two_Product_2Presplit(a, ahi, alo, b, bhi, blo, x, y) \
  x = (REAL) (a * b); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

/* Square() can be done more quickly than Two_Product().                     */

#define Square_Tail(a, x, y) \
  Split(a, ahi, alo); \
  err1 = x - (ahi * ahi); \
  err3 = err1 - ((ahi + ahi) * alo); \
  y = (alo * alo) - err3

#define Square(a, x, y) \
  x = (REAL) (a * a); \
  Square_Tail(a, x, y)

/* Macros for summing expansions of various fixed lengths.  These are all    */
/*   unrolled versions of Expansion_Sum().                                   */

#define Two_One_Sum(a1, a0, b, x2, x1, x0) \
  Two_Sum(a0, b , _i, x0); \
  Two_Sum(a1, _i, x2, x1)

#define Two_One_Diff(a1, a0, b, x2, x1, x0) \
  Two_Diff(a0, b , _i, x0); \
  Two_Sum( a1, _i, x2, x1)

#define Two_Two_Sum(a1, a0, b1, b0, x3, x2, x1, x0) \
  Two_One_Sum(a1, a0, b0, _j, _0, x0); \
  Two_One_Sum(_j, _0, b1, x3, x2, x1)

#define Two_Two_Diff(a1, a0, b1, b0, x3, x2, x1, x0) \
  Two_One_Diff(a1, a0, b0, _j, _0, x0); \
  Two_One_Diff(_j, _0, b1, x3, x2, x1)

#define Four_One_Sum(a3, a2, a1, a0, b, x4, x3, x2, x1, x0) \
  Two_One_Sum(a1, a0, b , _j, x1, x0); \
  Two_One_Sum(a3, a2, _j, x4, x3, x2)

#define Four_Two_Sum(a3, a2, a1, a0, b1, b0, x5, x4, x3, x2, x1, x0) \
  Four_One_Sum(a3, a2, a1, a0, b0, _k, _2, _1, _0, x0); \
  Four_One_Sum(_k, _2, _1, _0, b1, x5, x4, x3, x2, x1)

#define Four_Four_Sum(a3, a2, a1, a0, b4, b3, b1, b0, x7, x6, x5, x4, x3, x2, \
                      x1, x0) \
  Four_Two_Sum(a3, a2, a1, a0, b1, b0, _l, _2, _1, _0, x1, x0); \
  Four_Two_Sum(_l, _2, _1, _0, b4, b3, x7, x6, x5, x4, x3, x2)

#define Eight_One_Sum(a7, a6, a5, a4, a3, a2, a1, a0, b, x8, x7, x6, x5, x4, \
                      x3, x2, x1, x0) \
  Four_One_Sum(a3, a2, a1, a0, b , _j, x3, x2, x1, x0); \
  Four_One_Sum(a7, a6, a5, a4, _j, x8, x7, x6, x5, x4)

#define Eight_Two_Sum(a7, a6, a5, a4, a3, a2, a1, a0, b1, b0, x9, x8, x7, \
                      x6, x5, x4, x3, x2, x1, x0) \
  Eight_One_Sum(a7, a6, a5, a4, a3, a2, a1, a0, b0, _k, _6, _5, _4, _3, _2, \
                _1, _0, x0); \
  Eight_One_Sum(_k, _6, _5, _4, _3, _2, _1, _0, b1, x9, x8, x7, x6, x5, x4, \
                x3, x2, x1)

#define Eight_Four_Sum(a7, a6, a5, a4, a3, a2, a1, a0, b4, b3, b1, b0, x11, \
                       x10, x9, x8, x7, x6, x5, x4, x3, x2, x1, x0) \
  Eight_Two_Sum(a7, a6, a5, a4, a3, a2, a1, a0, b1, b0, _l, _6, _5, _4, _3, \
                _2, _1, _0, x1, x0); \
  Eight_Two_Sum(_l, _6, _5, _4, _3, _2, _1, _0, b4, b3, x11, x10, x9, x8, \
                x7, x6, x5, x4, x3, x2)

/* Macros for multiplying expansions of various fixed lengths.               */

#define Two_One_Product(a1, a0, b, x3, x2, x1, x0) \
  Split(b, bhi, blo); \
  Two_Product_Presplit(a0, b, bhi, blo, _i, x0); \
  Two_Product_Presplit(a1, b, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _k, x1); \
  Fast_Two_Sum(_j, _k, x3, x2)

#define Four_One_Product(a3, a2, a1, a0, b, x7, x6, x5, x4, x3, x2, x1, x0) \
  Split(b, bhi, blo); \
  Two_Product_Presplit(a0, b, bhi, blo, _i, x0); \
  Two_Product_Presplit(a1, b, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _k, x1); \
  Fast_Two_Sum(_j, _k, _i, x2); \
  Two_Product_Presplit(a2, b, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _k, x3); \
  Fast_Two_Sum(_j, _k, _i, x4); \
  Two_Product_Presplit(a3, b, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _k, x5); \
  Fast_Two_Sum(_j, _k, x7, x6)

#define Two_Two_Product(a1, a0, b1, b0, x7, x6, x5, x4, x3, x2, x1, x0) \
  Split(a0, a0hi, a0lo); \
  Split(b0, bhi, blo); \
  Two_Product_2Presplit(a0, a0hi, a0lo, b0, bhi, blo, _i, x0); \
  Split(a1, a1hi, a1lo); \
  Two_Product_2Presplit(a1, a1hi, a1lo, b0, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _k, _1); \
  Fast_Two_Sum(_j, _k, _l, _2); \
  Split(b1, bhi, blo); \
  Two_Product_2Presplit(a0, a0hi, a0lo, b1, bhi, blo, _i, _0); \
  Two_Sum(_1, _0, _k, x1); \
  Two_Sum(_2, _k, _j, _1); \
  Two_Sum(_l, _j, _m, _2); \
  Two_Product_2Presplit(a1, a1hi, a1lo, b1, bhi, blo, _j, _0); \
  Two_Sum(_i, _0, _n, _0); \
  Two_Sum(_1, _0, _i, x2); \
  Two_Sum(_2, _i, _k, _1); \
  Two_Sum(_m, _k, _l, _2); \
  Two_Sum(_j, _n, _k, _0); \
  Two_Sum(_1, _0, _j, x3); \
  Two_Sum(_2, _j, _i, _1); \
  Two_Sum(_l, _i, _m, _2); \
  Two_Sum(_1, _k, _i, x4); \
  Two_Sum(_2, _i, _k, x5); \
  Two_Sum(_m, _k, x7, x6)

/* An expansion of length two can be squared more quickly than finding the   */
/*   product of two different expansions of length two, and the result is    */
/*   guaranteed to have no more than six (rather than eight) components.     */

#define Two_Square(a1, a0, x5, x4, x3, x2, x1, x0) \
  Square(a0, _j, x0); \
  _0 = a0 + a0; \
  Two_Product(a1, _0, _k, _1); \
  Two_One_Sum(_k, _1, _j, _l, _2, x1); \
  Square(a1, _j, _1); \
  Two_Two_Sum(_j, _1, _l, _2, x5, x4, x3, x2)

REAL splitter;     /* = 2^ceiling(p / 2) + 1.  Used to split floats in half. */
REAL epsilon;                /* = 2^(-p).  Used to estimate roundoff errors. */
/* A set of coefficients used to calculate maximum roundoff errors.          */
REAL resulterrbound;
REAL ccwerrboundA, ccwerrboundB, ccwerrboundC;
REAL o3derrboundA, o3derrboundB, o3derrboundC;
REAL iccerrboundA, iccerrboundB, iccerrboundC;
REAL isperrboundA, isperrboundB, isperrboundC;




/*****************************************************************************/
/*                                                                           */
/*  fast_expansion_sum_zeroelim()   Sum two expansions, eliminating zero     */
/*                                  components from the output expansion.    */
/*                                                                           */
/*  Sets h = e + f.  See the long version of my paper for details.           */
/*                                                                           */
/*  If round-to-even is used (as with IEEE 754), maintains the strongly      */
/*  nonoverlapping property.  (That is, if e is strongly nonoverlapping, h   */
/*  will be also.)  Does NOT maintain the nonoverlapping or nonadjacent      */
/*  properties.                                                              */
/*                                                                           */
/*****************************************************************************/

#if 0
static int fast_expansion_sum_zeroelim(elen, e, flen, f, h)  /* h cannot be e or f. */
int elen;
REAL *e;
int flen;
REAL *f;
REAL *h;
{
  REAL Q;
  INEXACT REAL Qnew;
  INEXACT REAL hh;
  INEXACT REAL bvirt;
  REAL avirt, bround, around;
  int eindex, findex, hindex;
  REAL enow, fnow;

  enow = e[0];
  fnow = f[0];
  eindex = findex = 0;
  if ((fnow > enow) == (fnow > -enow)) {
    Q = enow;
    enow = e[++eindex];
  } else {
    Q = fnow;
    fnow = f[++findex];
  }
  hindex = 0;
  if ((eindex < elen) && (findex < flen)) {
    if ((fnow > enow) == (fnow > -enow)) {
      Fast_Two_Sum(enow, Q, Qnew, hh);
      enow = e[++eindex];
    } else {
      Fast_Two_Sum(fnow, Q, Qnew, hh);
      fnow = f[++findex];
    }
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
    while ((eindex < elen) && (findex < flen)) {
      if ((fnow > enow) == (fnow > -enow)) {
        Two_Sum(Q, enow, Qnew, hh);
        enow = e[++eindex];
      } else {
        Two_Sum(Q, fnow, Qnew, hh);
        fnow = f[++findex];
      }
      Q = Qnew;
      if (hh != 0.0) {
        h[hindex++] = hh;
      }
    }
  }
  while (eindex < elen) {
    Two_Sum(Q, enow, Qnew, hh);
    enow = e[++eindex];
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
  }
  while (findex < flen) {
    Two_Sum(Q, fnow, Qnew, hh);
    fnow = f[++findex];
    Q = Qnew;
    if (hh != 0.0) {
      h[hindex++] = hh;
    }
  }
  if ((Q != 0.0) || (hindex == 0)) {
    h[hindex++] = Q;
  }
  return hindex;
}
#endif




/*****************************************************************************/
/*                                                                           */
/*  orient2dfast()   Approximate 2D orientation test.  Nonrobust.            */
/*  orient2dexact()   Exact 2D orientation test.  Robust.                    */
/*  orient2dslow()   Another exact 2D orientation test.  Robust.             */
/*  orient2d()   Adaptive exact 2D orientation test.  Robust.                */
/*                                                                           */
/*               Return a positive value if the points pa, pb, and pc occur  */
/*               in counterclockwise order; a negative value if they occur   */
/*               in clockwise order; and zero if they are collinear.  The    */
/*               result is also a rough approximation of twice the signed    */
/*               area of the triangle defined by the three points.           */
/*                                                                           */
/*  Only the first and last routine should be used; the middle two are for   */
/*  timings.                                                                 */
/*                                                                           */
/*  The last three use exact arithmetic to ensure a correct answer.  The     */
/*  result returned is the determinant of a matrix.  In orient2d() only,     */
/*  this determinant is computed adaptively, in the sense that exact         */
/*  arithmetic is used only to the degree it is needed to ensure that the    */
/*  returned value has the correct sign.  Hence, orient2d() is usually quite */
/*  fast, but will run more slowly when the input points are collinear or    */
/*  nearly so.                                                               */
/*                                                                           */
/*****************************************************************************/




/*****************************************************************************/
/*                                                                           */
/*  orient3dfast()   Approximate 3D orientation test.  Nonrobust.            */
/*  orient3dexact()   Exact 3D orientation test.  Robust.                    */
/*  orient3dslow()   Another exact 3D orientation test.  Robust.             */
/*  orient3d()   Adaptive exact 3D orientation test.  Robust.                */
/*                                                                           */
/*               Return a positive value if the point pd lies below the      */
/*               plane passing through pa, pb, and pc; "below" is defined so */
/*               that pa, pb, and pc appear in counterclockwise order when   */
/*               viewed from above the plane.  Returns a negative value if   */
/*               pd lies above the plane.  Returns zero if the points are    */
/*               coplanar.  The result is also a rough approximation of six  */
/*               times the signed volume of the tetrahedron defined by the   */
/*               four points.                                                */
/*                                                                           */
/*  Only the first and last routine should be used; the middle two are for   */
/*  timings.                                                                 */
/*                                                                           */
/*  The last three use exact arithmetic to ensure a correct answer.  The     */
/*  result returned is the determinant of a matrix.  In orient3d() only,     */
/*  this determinant is computed adaptively, in the sense that exact         */
/*  arithmetic is used only to the degree it is needed to ensure that the    */
/*  returned value has the correct sign.  Hence, orient3d() is usually quite */
/*  fast, but will run more slowly when the input points are coplanar or     */
/*  nearly so.                                                               */
/*                                                                           */
/*****************************************************************************/

static REAL orient3dfast(pa, pb, pc, pd)
REAL *pa;
REAL *pb;
REAL *pc;
REAL *pd;
{
  REAL adx, bdx, cdx;
  REAL ady, bdy, cdy;
  REAL adz, bdz, cdz;

  adx = pa[0] - pd[0];
  bdx = pb[0] - pd[0];
  cdx = pc[0] - pd[0];
  ady = pa[1] - pd[1];
  bdy = pb[1] - pd[1];
  cdy = pc[1] - pd[1];
  adz = pa[2] - pd[2];
  bdz = pb[2] - pd[2];
  cdz = pc[2] - pd[2];

  return adx * (bdy * cdz - bdz * cdy)
       + bdx * (cdy * adz - cdz * ady)
       + cdx * (ady * bdz - adz * bdy);
}





/*****************************************************************************/
/*                                                                           */
/*  inspherefast()   Approximate 3D insphere test.  Nonrobust.               */
/*  insphereexact()   Exact 3D insphere test.  Robust.                       */
/*  insphereslow()   Another exact 3D insphere test.  Robust.                */
/*  insphere()   Adaptive exact 3D insphere test.  Robust.                   */
/*                                                                           */
/*               Return a positive value if the point pe lies inside the     */
/*               sphere passing through pa, pb, pc, and pd; a negative value */
/*               if it lies outside; and zero if the five points are         */
/*               cospherical.  The points pa, pb, pc, and pd must be ordered */
/*               so that they have a positive orientation (as defined by     */
/*               orient3d()), or the sign of the result will be reversed.    */
/*                                                                           */
/*  Only the first and last routine should be used; the middle two are for   */
/*  timings.                                                                 */
/*                                                                           */
/*  The last three use exact arithmetic to ensure a correct answer.  The     */
/*  result returned is the determinant of a matrix.  In insphere() only,     */
/*  this determinant is computed adaptively, in the sense that exact         */
/*  arithmetic is used only to the degree it is needed to ensure that the    */
/*  returned value has the correct sign.  Hence, insphere() is usually quite */
/*  fast, but will run more slowly when the input points are cospherical or  */
/*  nearly so.                                                               */
/*                                                                           */
/*****************************************************************************/

static REAL inspherefast(pa, pb, pc, pd, pe)
REAL *pa;
REAL *pb;
REAL *pc;
REAL *pd;
REAL *pe;
{
  REAL aex, bex, cex, dex;
  REAL aey, bey, cey, dey;
  REAL aez, bez, cez, dez;
  REAL alift, blift, clift, dlift;
  REAL ab, bc, cd, da, ac, bd;
  REAL abc, bcd, cda, dab;

  aex = pa[0] - pe[0];
  bex = pb[0] - pe[0];
  cex = pc[0] - pe[0];
  dex = pd[0] - pe[0];
  aey = pa[1] - pe[1];
  bey = pb[1] - pe[1];
  cey = pc[1] - pe[1];
  dey = pd[1] - pe[1];
  aez = pa[2] - pe[2];
  bez = pb[2] - pe[2];
  cez = pc[2] - pe[2];
  dez = pd[2] - pe[2];

  ab = aex * bey - bex * aey;
  bc = bex * cey - cex * bey;
  cd = cex * dey - dex * cey;
  da = dex * aey - aex * dey;

  ac = aex * cey - cex * aey;
  bd = bex * dey - dex * bey;

  abc = aez * bc - bez * ac + cez * ab;
  bcd = bez * cd - cez * bd + dez * bc;
  cda = cez * da + dez * ac + aez * cd;
  dab = dez * ab + aez * bd + bez * da;

  alift = aex * aex + aey * aey + aez * aez;
  blift = bex * bex + bey * bey + bez * bez;
  clift = cex * cex + cey * cey + cez * cez;
  dlift = dex * dex + dey * dey + dez * dez;

  return (dlift * abc - clift * dab) + (blift * cda - alift * bcd);
}



////////////////////////////////// end of predicates.c /////////////////////////////////

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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "assert.h"
#include <time.h>

/* These macros make code more readable. They allow us to access
   the indexed elements of verticies directly.                    */

static void             getRange(GwyDelaunayVertex *ps, gint n, GwyDelaunayVertex *min,
                                             GwyDelaunayVertex *max, GwyDelaunayVertex *range, gint r);
static void             initSuperSimplex(GwyDelaunayVertex *ps, gint n, GwyDelaunayMesh *m);
static void             getFaceVerticies(simplex *s, gint i, GwyDelaunayVertex **p1, GwyDelaunayVertex **p2,
                                                     GwyDelaunayVertex **p3, GwyDelaunayVertex **p4 );
static void             addSimplexToMesh(GwyDelaunayMesh *m, simplex *s);
static void             removeSimplexFromMesh(GwyDelaunayMesh *m, simplex *s);
static simplex*         findContainingSimplex(GwyDelaunayMesh *m, GwyDelaunayVertex *p);
static gint              isDelaunay(simplex *s, GwyDelaunayVertex *p);
static simplex**        swapSimplexNeighbour(simplex *s, simplex *old, simplex *new);
static arrayList*       findNeighbours(GwyDelaunayVertex *v, simplex *s);
static simplex*         newSimplex(GwyDelaunayMesh *m);
static void             circumCenter(simplex *s, gdouble *out);
static void             setNeighbours(arrayList *newTets);
static void             vertexAdd(gdouble *a, gdouble *b, gdouble *out);
static void             vertexSub(gdouble *a, gdouble *b, gdouble *out);
static void             crossProduct(gdouble *b, gdouble *c, gdouble *out);
static gdouble           squaredDistance(gdouble *a);
static gdouble           scalarProduct(gdouble *a, gdouble *b);
static gdouble           volumeOfTetrahedron(gdouble *a,gdouble *b, gdouble *c, gdouble *d);
static neighbourUpdate* initNeighbourUpdates();
static void             resetNeighbourUpdates(neighbourUpdate *nu);
static void             undoNeighbourUpdates(neighbourUpdate *nu);
static void             pushNeighbourUpdate(neighbourUpdate *nu, simplex **ptr,
                                                          simplex  *old);
static void             freeNeighbourUpdates(neighbourUpdate *nu);
static void             randomPerturbation(GwyDelaunayVertex *v, gint attempt);


/******************************************************************************/

/* Set this to be lower than the average distance between points. It is the
   amount that we will shift points by when we detect degenerecies. We
   gradually increase the value until the degenercy is removed                */
  #define PERTURBATION_VALUE  1e-9


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

void _gwy_delaunay_mesh_build(GwyDelaunayMesh *m, GwyDelaunayVertex* ps, gint n)
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
   // printf("orientation error: %p, %lf\n",s,orientation);

   // exit(1);
    return -1;
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

static void gwy_delaunay_add_point(GwyDelaunayVertex *p, GwyDelaunayMesh *m)
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
// Does this simplex have the point p? - notice that we are comparing pointersa
// and not coordinates: so that duplicate coordinates will evaluate to not
// equal.

static gint gwy_delaunay_point_on_simplex(GwyDelaunayVertex *p, simplex *s)
{
  if (!s) return 0;

  if (p == s->p[0] || p == s->p[1] || p == s->p[2] || p == s->p[3])
    return 1;

  return 0;
}



/******************************************************************************/
// Given a point and a list of simplicies, we want to find any valid
// neighbour of this point.

static simplex *gwy_delaunay_find_any_neighbour(GwyDelaunayVertex *v, arrayList *tets)
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

static void gwy_delaunay_remove_point(GwyDelaunayMesh *m)
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

static gdouble gwy_delaunay_voronoi_cell_volume(voronoiCell *vc, GwyDelaunayVertex *p)
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
// This will give us the volume of the voronoi cell about the point p.
// We pass a point, at least one simplex containing that point, and the mesh.

static voronoiCell* gwy_delaunay_get_voronoi_cell(GwyDelaunayVertex *point, simplex *s0, GwyDelaunayMesh *m)
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
 //   if (!simplexContainsPoint(m->super, point))
 //     fprintf(stderr,"Error: point outside of delaunay triangulation. - "
 //                    "try extending the super-simplex and re-starting.\n");
 //   else
 //    fprintf(stderr, "Error: No neighbours found for point! - mesh appears "
 //                    "to be degenerate.\n");
 //   exit(1);
      return NULL;
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

static void gwy_delaunay_free_voronoi_cell(voronoiCell *vc, GwyDelaunayMesh *m)
{
  // We just push the cell to the memory pool.
  // We can free the memory pools manually, or let the program do it
  // automatically at the end.
  push(m->deadVoronoiCells, vc);
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

GwyDelaunayMesh *_gwy_delaunay_mesh_new()
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

void _gwy_delaunay_mesh_free(GwyDelaunayMesh *m)
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

static void gwy_delaunay_vertex_by_scalar(gdouble *a, gdouble b, gdouble *out)
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

static void getRange(GwyDelaunayVertex *ps, gint n, GwyDelaunayVertex *min, GwyDelaunayVertex *max, GwyDelaunayVertex *range, G_GNUC_UNUSED gint r)
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



/******************************************************************************/

GwyDelaunayVertex *_gwy_delaunay_vertex_new(gdouble *x, gdouble *y, gdouble *z,
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


static void lastNaturalNeighbours(GwyDelaunayVertex *v, GwyDelaunayMesh *m, arrayList *neighbours,
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

void _gwy_delaunay_mesh_interpolate3_3(GwyDelaunayMesh *m, gdouble  x, gdouble  y, gdouble  z,
                     gdouble *u, gdouble *v, gdouble *w)
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
    //fprintf(stderr, "Error: sum value: %lf, expected range (0,1].\n",sum);
    //fprintf(stderr, "There could be a degenerecy in the mesh, either retry "
    //                "(since input is randomised this may resolve the problem), "
    //                "or try adding a random peterbation to every point.\n");
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



