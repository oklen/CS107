#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation)
{
  //v = malloc(sizeof(vector));
   v->elementSize = elemSize;

   //printf("Size %d\n", v->elementSize);
   v->elementsAddr = malloc(initialAllocation > 0?initialAllocation*elemSize:1*elemSize);
   v->backPos = 0;
   v->currentLength = initialAllocation > 0? initialAllocation : 1;
   v->freeFun = freeFn;
}

void VectorDispose(vector *v)
{
  if(v->freeFun != NULL)
    v->freeFun(v->elementsAddr);
}

int VectorLength(const vector *v)
{
  return v->backPos;
}

void *VectorNth(const vector *v, int position)
{
  return (char*)v->elementsAddr + position*v->elementSize;
}

void VectorReplace(vector *v, const void *elemAddr, int position)
{
  
  memcpy((char*)v->elementsAddr + position*v->elementSize,elemAddr,v->elementSize);
}

void VectorInsert(vector *v, const void *elemAddr, int position)
{
  if(v->backPos + 1 > v->currentLength)
    {
      void *reallocRes =  realloc(v->elementsAddr, v->elementSize * v->currentLength *2);
      if(reallocRes == NULL)
        {
          reallocRes = v->elementsAddr;
          v->elementsAddr =  malloc(v->elementSize*v->currentLength * 2);
          assert(v->elementsAddr != NULL);
          memcpy(v->elementsAddr, reallocRes, v->elementSize*v->currentLength);
          free(reallocRes);
        }
      v->currentLength = v->currentLength * 2;
    }
  for(int i = v ->backPos + 1;i > position;i--)
    {
      memcpy( (char*)v->elementsAddr + v->elementSize * i, (char*)v->elementsAddr + v->elementSize * (i-1) , v->elementSize);
    }
  memcpy((char*)v->elementsAddr + v->elementSize * position, elemAddr, v->elementSize);
  ++v->backPos;
}

void VectorAppend(vector *v, const void *elemAddr)
{
  //printf("GT:%d \n",v->elementSize);
  //printf("%d %d DEB\n",v->currentLength,v->backPos);
  if(v->backPos + 1 > v->currentLength)
    {
      //printf("%d\n",  v->elementSize * v->currentLength );
      void *reallocRes =  realloc(v->elementsAddr, v->elementSize * v->currentLength *2);
      if(reallocRes == NULL)
        {

          reallocRes = v->elementsAddr;
          //printf("%d %d DEB\n",v->currentLength,v->backPos);
          v->elementsAddr =  malloc( v->elementSize*v->currentLength * 2LL);
          assert(v->elementsAddr != NULL);
          memcpy(v->elementsAddr, reallocRes, v->elementSize*v->currentLength);
          free(reallocRes);
        }
      else
        v->elementsAddr = reallocRes;

      v->currentLength = v->currentLength * 2;
    }
  memcpy((char*)v->elementsAddr + v->elementSize * v->backPos, elemAddr, v->elementSize);
  //printf("Doing CPY DONE\n");
  v->backPos++;
}

void VectorDelete(vector *v, int position)
{
  for(int i = position;i < v->backPos;i++)
    {
      memcpy((char*)v->elementsAddr + v->elementSize * i,(char*)v->elementsAddr + v->elementSize * (i+1) , v->elementSize);
    }
  v->backPos--;
  if(v->backPos < v->currentLength /4)
    {
      v->elementsAddr = realloc(v->elementsAddr, v->elementSize * v->currentLength /2);
      v->currentLength = v->currentLength /2;
    }
}

void VectorSort(vector *v, VectorCompareFunction compare)
{
  qsort(v->elementsAddr, v->backPos, v->elementSize, compare);
}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{
  for(int i = 0; i < v->backPos;i++)
    {
      mapFn((char*)v->elementsAddr + v->elementSize * i,auxData);
    }
}

static const int kNotFound = -1;
int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{
  
  assert(startIndex >= 0 || startIndex <= v->backPos || key == NULL);
  void* res = NULL;
  if(isSorted)
    {
      res = bsearch(key, (char*)v->elementsAddr + v->elementSize * startIndex, v->backPos, v->elementSize, searchFn);
    }
  else
    {
      for(int i = startIndex; i < v->backPos;i++)
        {
          if(searchFn((char*)v->elementsAddr + v->elementSize*i,key) == 0)
            {
              res = (char*)v->elementsAddr + v->elementSize*i;
              break;
            }
        }
    }

  if(res == NULL)
    return kNotFound;
  else
    return (res - v->elementsAddr)/v->elementSize;
}
