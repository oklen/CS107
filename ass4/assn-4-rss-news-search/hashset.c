#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn)
{
  assert(elemSize > 0 && numBuckets > 0 && hashfn != NULL && comparefn != NULL);
  h->elementSize = elemSize;
  h->bucketsLength = numBuckets;
  h->compareFn = comparefn;
  h->freeFn = freefn;
  h->setFn = hashfn;
  h->dataarray = malloc(sizeof(vector) * numBuckets);
  memset(h->dataarray,0 , sizeof(vector) * numBuckets);
  h->backPos = 0;
  for(int i = 0; i < numBuckets ;i++)
    {
      //printf("size:%d\n",elemSize);
      VectorNew((vector*)((char*)((void*)h->dataarray) + sizeof(vector) * i), elemSize, freefn, 1);
      //h->backPos++;
    }
  //printf("Init %d bucket\n",h->bucketsLength);
}

void HashSetDispose(hashset *h)
{
  for(int i=0;i < h->bucketsLength ; i++)
    VectorDispose((((vector*)((char*)(void*)h->dataarray + sizeof(vector) * i))));
  free(h->dataarray);
}

int HashSetCount(const hashset *h)
{ return h->backPos; }

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{
  for(int i = 0 ; i < 200; i++)
    {
      VectorMap((vector*)((char*)h->dataarray + sizeof(vector) * i), mapfn,auxData);
    }
}

void HashSetEnter(hashset *h, const void *elemAddr)
{
  //printf("Generate Index: %d BackPos:%d\n",h->setFn(elemAddr,h->bucketsLength),h->dataarray[h->setFn(elemAddr,h->bucketsLength)].backPos);

  if(VectorLength(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)]) != 0){
    int res = VectorSearch( &h->dataarray[h->setFn(elemAddr,h->bucketsLength)], elemAddr, h->compareFn, 0, false);
    //printf("Get res:%d %d ",res,*(int*)elemAddr);
    if(res == -1)
      {
        VectorAppend(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)], elemAddr);
        h->backPos++;
      }
    else
      {
        VectorReplace(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)], elemAddr, res);
      }
  }
  else
    {
      //printf("Zero ADD\n");
      VectorAppend(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)], elemAddr);
      h->backPos++;
    }
}

void *HashSetLookup(const hashset *h, const void *elemAddr)
{
  assert(elemAddr != NULL);
  //printf("%s %d\n",elemAddr,h->dataarray[h->setFn(elemAddr,h->bucketsLength)].backPos);
  if(VectorLength(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)]) != 0)
    {

      int res = VectorSearch(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)], elemAddr, h->compareFn, 0, false);
      //printf("Look for %s Res: %d\n",elemAddr,res);
      if(res == -1)
        return NULL;
      else
        {
          //printf("Found:%s\n",elemAddr);
          return VectorNth(&h->dataarray[h->setFn(elemAddr,h->bucketsLength)],res);
        }
    }
  else
    {
      //printf("Zero return\n");
      return NULL;
    }
}
