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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
* Array list functions.
*******************************************************************************/
static gint         addToArrayList(arrayList *l, void* element);
static void*       getFromArrayList (arrayList *l, gint index);
static void**      getArrayFromArrayList(arrayList *l);
static gint         arrayListGetIndex(arrayList *l, void *e);
static gint         arrayListSize(arrayList *l);
static arrayList*  newArrayList();
static void        freeArrayList(arrayList *l, void (*destructor)(void *e));
static gint         arrayListContains(arrayList * l , void * element);
static void        freeElements(arrayList *l);
static void        emptyArrayList(arrayList *l);

/*******************************************************************************
* Doubly linked list functions.
*******************************************************************************/
static linkedList* newLinkedList();
static listNode*   addToLinkedList(linkedList *l, void *e);
static void*       getFromLinkedList(linkedList *l, gint i);
static gint         linkedListSize(linkedList *l);
static void*       nextElement(linkedList *l, listNode **last);
static listNode*   topOfLinkedList(linkedList *l);
static void        removeFromLinkedList(linkedList *l, listNode *ln);
static void        freeLinkedList(linkedList *l, void (*destructor)(void *e));
static gint         linkedListContains(linkedList *l, void *e);

/*******************************************************************************
* Stack functions.
*******************************************************************************/
static stack*      newStack();
static void        push(stack *s, void*e);
static void*       pop(stack *s);
static gint         stackSize(stack *s);
static void        freeStack(stack *s, void (*destructor)(void *e));
static gint         isEmpty(stack *s);
static void        emptyStack(stack *s);


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
         fprintf(stderr, "Error: Out of Memory.\n");
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
// If this element is in the list, it will return the index of that element.
// Otherwise we return -1.

static gint arrayListGetIndex(arrayList *l, void *e)
{
  gint i;
  for (i=0; i<arrayListSize(l); i++)
    if (getFromArrayList(l, i) == e) return i;
  return -1;
}

/******************************************************************************/

static void** getArrayFromArrayList(arrayList *l)
{
   return l->arr;
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

static void * getFromArrayList (arrayList *l, gint index)
{
   if(index >= 0 && index <  l->num_elements)
      return l->arr[index];
      
   return NULL;
}

/******************************************************************************/

static void freeElements(arrayList *l)
{
  gint i;
  
  for (i=0; i<arrayListSize(l); i++)
    free(getFromArrayList(l,i));

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

static void *getFromLinkedList(linkedList *l, gint x)
{

  listNode *thisNode;
  gint i;

  if (! (0 <= x && x < linkedListSize(l)) )
  {
    fprintf(stderr, "list index out of bounds, linkedList-size: %d, index: %d.\n", 
      linkedListSize(l), x);
    exit(1);
  }
  
  thisNode = topOfLinkedList(l);
  for (i=0; i<x; i++)
    thisNode = thisNode->next;
  return thisNode->data;

}

/******************************************************************************/

static gint linkedListSize(linkedList *l)
{
  return l->nelem;
}

/******************************************************************************/

static void *prevElement(linkedList *l, listNode **last)
{
  void *e;
  // If this is the end, return null.
  if (!*last) return NULL;
  // Move the iterator along to the next element,
  // and then return the data item 
  e = (*last)->data;
  *last = (*last)->prev;
  return e;
}

/******************************************************************************/

static void *nextElement(linkedList *l, listNode **last)
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

static gint linkedListContains(linkedList *l, void *e)
{
  listNode *iter = topOfLinkedList(l);
  void *this;
  
  while((this = nextElement(l, &iter)))
    if (this==e) return 1;
  
  return 0;
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
    fprintf(stderr, "Error: Tried to remove null element from linkedList.\n");
    exit(1);
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

static gint stackSize(stack *s)
{
  return s->top;
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
         fprintf(stderr, "Error: Out of Memory.\n");
         exit(1);
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



