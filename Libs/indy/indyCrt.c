#include "indyCrt.h"

#include <stdint.h>

// Ranges of up to this many elements are sorted by selection sort instead of being partitioned
#define INDYCRT_QSORT_SMALLRANGE 8

// Pending ranges. The larger part of every partition is the one deferred, so each deferred range is less than half
// the size of the one before: 30 entries suffice for any element count that fits in 32 bits.
#define INDYCRT_QSORT_MAXPENDING 30

static void indyCrt_SwapElements(uint8_t* pA, uint8_t* pB, size_t width)
{
    if ( pA == pB )
    {
        return;
    }

    for ( size_t i = 0; i < width; i++ )
    {
        uint8_t tmp = pA[i];
        pA[i] = pB[i];
        pB[i] = tmp;
    }
}

// Sorts the elements from pFirst to pLast (inclusive) by moving the greatest remaining element to the end, again and
// again. Of equal greatest elements the first one is moved.
static void indyCrt_SelectionSort(uint8_t* pFirst, uint8_t* pLast, size_t width, int (__cdecl* pfCompare)(const void*, const void*))
{
    while ( pLast > pFirst )
    {
        uint8_t* pMax = pFirst;
        for ( uint8_t* pCur = pFirst + width; pCur <= pLast; pCur += width )
        {
            if ( pfCompare(pCur, pMax) > 0 )
            {
                pMax = pCur;
            }
        }

        indyCrt_SwapElements(pMax, pLast, width);
        pLast -= width;
    }
}

void indyCrt_Qsort(void* pBase, size_t num, size_t width, int (__cdecl* pfCompare)(const void*, const void*))
{
    if ( num < 2 || width == 0 )
    {
        return;
    }

    uint8_t* aPendingFirst[INDYCRT_QSORT_MAXPENDING];
    uint8_t* aPendingLast[INDYCRT_QSORT_MAXPENDING];
    int numPending = 0;

    uint8_t* pFirst = (uint8_t*)pBase;
    uint8_t* pLast  = (uint8_t*)pBase + (num - 1) * width;
    for ( ;; )
    {
        size_t count = (size_t)(pLast - pFirst) / width + 1;
        if ( count <= INDYCRT_QSORT_SMALLRANGE )
        {
            indyCrt_SelectionSort(pFirst, pLast, width, pfCompare);
        }
        else
        {
            // The middle element is the pivot; it waits at the front while the rest is partitioned
            indyCrt_SwapElements(pFirst + (count / 2) * width, pFirst, width);

            uint8_t* pLow  = pFirst;
            uint8_t* pHigh = pLast + width;
            for ( ;; )
            {
                do
                {
                    pLow += width;
                } while ( pLow <= pLast && pfCompare(pLow, pFirst) <= 0 );

                do
                {
                    pHigh -= width;
                } while ( pHigh > pFirst && pfCompare(pHigh, pFirst) >= 0 );

                if ( pHigh < pLow )
                {
                    break;
                }

                indyCrt_SwapElements(pLow, pHigh, width);
            }

            // Pivot into its final place: elements before pHigh are <= pivot, elements from pLow on are >= pivot
            indyCrt_SwapElements(pFirst, pHigh, width);

            // Continue with the smaller part, defer the larger one
            if ( (pHigh - pFirst) - 1 >= pLast - pLow )
            {
                if ( pFirst + width < pHigh )
                {
                    aPendingFirst[numPending] = pFirst;
                    aPendingLast[numPending]  = pHigh - width;
                    numPending++;
                }

                if ( pLow < pLast )
                {
                    pFirst = pLow;
                    continue;
                }
            }
            else
            {
                if ( pLow < pLast )
                {
                    aPendingFirst[numPending] = pLow;
                    aPendingLast[numPending]  = pLast;
                    numPending++;
                }

                if ( pFirst + width < pHigh )
                {
                    pLast = pHigh - width;
                    continue;
                }
            }
        }

        if ( numPending == 0 )
        {
            return;
        }

        numPending--;
        pFirst = aPendingFirst[numPending];
        pLast  = aPendingLast[numPending];
    }
}
