/*
 * multi_heap.h (completion for heap_4.c / h)
 *
 *  Created on: Dec 14, 2020
 *      Author: Benjami
 */

/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of http://www.FreeRTOS.org for more information.
 */
#include <stdlib.h>
#include <string.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "main.h"

#ifdef  osCMSIS
#include "FreeRTOS.h"
#include "task.h"
#else
#define portBYTE_ALIGNMENT   8
#define portBYTE_ALIGNMENT_MASK (portBYTE_ALIGNMENT - 1)
#define configASSERT(x) if ((x) == 0) {for(;;);}
#define mtCOVERAGE_TEST_MARKER()
#ifndef traceMALLOC
#define traceMALLOC(pvAddress, uiSize)
#endif
#ifndef traceFREE
#define traceFREE(pvAddress, uiSize)
#endif
#define vTaskSuspendAll()
#define xTaskResumeAll()

typedef struct HeapRegion
{
  uint8_t *pucStartAddress;
  size_t xSizeInBytes;
} HeapRegion_t;
#endif

#include "multi_heap.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	((size_t) (xHeapStructSize << 1))

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		((size_t) 8)

/* Define the linked list structure.  This is used to link free blocks in order of their memory address. */
typedef struct A_BLOCK_LINK
{
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< The next free block in the list. */
	size_t xBlockSize;						/*<< The size of the free block. */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList(uint32_t i, BlockLink_t *pxBlockToInsert);

/*
 * Called automatically to setup the required heap structures the first time
 * pvPortMalloc() is called.
 */
static void prvHeapInit(uint32_t);

/* which memory region does the pv pointer point to? */
int32_t regionsearch(void * pv);

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const size_t xHeapStructSize	= (sizeof(BlockLink_t) + ((size_t) (portBYTE_ALIGNMENT - 1))) & ~((size_t) portBYTE_ALIGNMENT_MASK);

/* Create a couple of list links to mark the start and end of the list. */

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining[HEAP_NUM];
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM];

#ifdef HEAP_SHARED
static uint8_t HEAP_SHARED;
#endif

#if    HEAP_NUM == 1
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U};
#define DEFPARAM_0_(a)                   a
#define DEFPARAM_1_(a)                   -1
#define DEFPARAM_2_(a)                   -1
#define DEFPARAM_3_(a)                   -1
#define DEFPARAM_4_(a)                   -1
#define DEFPARAM_5_(a)                   -1
#define DEFPARAM_6_(a)                   -1
#define DEFPARAM_7_(a)                   -1
#elif  HEAP_NUM == 2
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U};
#define DEFPARAM_0_(a, b)                a
#define DEFPARAM_1_(a, b)                b
#define DEFPARAM_2_(a, b)                -1
#define DEFPARAM_3_(a, b)                -1
#define DEFPARAM_4_(a, b)                -1
#define DEFPARAM_5_(a, b)                -1
#define DEFPARAM_6_(a, b)                -1
#define DEFPARAM_7_(a, b)                -1
#elif  HEAP_NUM == 3
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c)             a
#define DEFPARAM_1_(a, b, c)             b
#define DEFPARAM_2_(a, b, c)             c
#define DEFPARAM_3_(a, b, c)             -1
#define DEFPARAM_4_(a, b, c)             -1
#define DEFPARAM_5_(a, b, c)             -1
#define DEFPARAM_6_(a, b, c)             -1
#define DEFPARAM_7_(a, b, c)             -1
#elif  HEAP_NUM == 4
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c, d)          a
#define DEFPARAM_1_(a, b, c, d)          b
#define DEFPARAM_2_(a, b, c, d)          c
#define DEFPARAM_3_(a, b, c, d)          d
#define DEFPARAM_4_(a, b, c, d)          -1
#define DEFPARAM_5_(a, b, c, d)          -1
#define DEFPARAM_6_(a, b, c, d)          -1
#define DEFPARAM_7_(a, b, c, d)          -1
#elif  HEAP_NUM == 5
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c, d, e)       a
#define DEFPARAM_1_(a, b, c, d, e)       b
#define DEFPARAM_2_(a, b, c, d, e)       c
#define DEFPARAM_3_(a, b, c, d, e)       d
#define DEFPARAM_4_(a, b, c, d, e)       e
#define DEFPARAM_5_(a, b, c, d, e)       -1
#define DEFPARAM_6_(a, b, c, d, e)       -1
#define DEFPARAM_7_(a, b, c, d, e)       -1
#elif  HEAP_NUM == 6
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL, NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c, d, e, f)    a
#define DEFPARAM_1_(a, b, c, d, e, f)    b
#define DEFPARAM_2_(a, b, c, d, e, f)    c
#define DEFPARAM_3_(a, b, c, d, e, f)    d
#define DEFPARAM_4_(a, b, c, d, e, f)    e
#define DEFPARAM_5_(a, b, c, d, e, f)    f
#define DEFPARAM_6_(a, b, c, d, e, f)    -1
#define DEFPARAM_7_(a, b, c, d, e, f)    -1
#elif  HEAP_NUM == 7
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c, d, e, f, g)    a
#define DEFPARAM_1_(a, b, c, d, e, f, g)    b
#define DEFPARAM_2_(a, b, c, d, e, f, g)    c
#define DEFPARAM_3_(a, b, c, d, e, f, g)    d
#define DEFPARAM_4_(a, b, c, d, e, f, g)    e
#define DEFPARAM_5_(a, b, c, d, e, f, g)    f
#define DEFPARAM_6_(a, b, c, d, e, f, g)    g
#define DEFPARAM_7_(a, b, c, d, e, f, g)    -1
#elif  HEAP_NUM == 8
static BlockLink_t xStart[HEAP_NUM], *pxEnd[HEAP_NUM] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static size_t xFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
static size_t xMinimumEverFreeBytesRemaining[HEAP_NUM] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
#define DEFPARAM_0_(a, b, c, d, e, f, g, h) a
#define DEFPARAM_1_(a, b, c, d, e, f, g, h) b
#define DEFPARAM_2_(a, b, c, d, e, f, g, h) c
#define DEFPARAM_3_(a, b, c, d, e, f, g, h) d
#define DEFPARAM_4_(a, b, c, d, e, f, g, h) e
#define DEFPARAM_5_(a, b, c, d, e, f, g, h) f
#define DEFPARAM_6_(a, b, c, d, e, f, g, h) g
#define DEFPARAM_7_(a, b, c, d, e, f, g, h) h
#endif

#define DEFPARAM_0(a)    DEFPARAM_0_(a)
#define DEFPARAM_1(a)    DEFPARAM_1_(a)
#define DEFPARAM_2(a)    DEFPARAM_2_(a)
#define DEFPARAM_3(a)    DEFPARAM_3_(a)
#define DEFPARAM_4(a)    DEFPARAM_4_(a)
#define DEFPARAM_5(a)    DEFPARAM_5_(a)
#define DEFPARAM_6(a)    DEFPARAM_6_(a)
#define DEFPARAM_7(a)    DEFPARAM_7_(a)

const HeapRegion_t       ucHeap[] = HEAP_REGIONS;

/* Gets set to the top bit of an size_t type.  When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application.  When the bit is free the block is still part of the free heap
space. */
static size_t xBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/

void *multiRegionMalloc(uint32_t i, size_t xWantedSize)
{
  BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
  void *pvReturn = NULL;

  /* if illegal memory region -> return NULL */
  if(i >= HEAP_NUM)
    return pvReturn;

	vTaskSuspendAll();
	{
		/* If this is the first call to malloc then the heap will require initialisation to setup the list of free blocks. */
		if(pxEnd[i] == NULL)
		{
			prvHeapInit(i);
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if((xWantedSize & xBlockAllocatedBit) == 0)
		{
			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if(xWantedSize > 0)
			{
				xWantedSize += xHeapStructSize;

				/* Ensure that blocks are always aligned to the required number	of bytes. */
				if((xWantedSize & portBYTE_ALIGNMENT_MASK) != 0x00)
				{
					/* Byte alignment required. */
					xWantedSize += (portBYTE_ALIGNMENT - (xWantedSize & portBYTE_ALIGNMENT_MASK));
					configASSERT((xWantedSize & portBYTE_ALIGNMENT_MASK) == 0);
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if((xWantedSize > 0) && (xWantedSize <= xFreeBytesRemaining[i]))
			{
				/* Traverse the list from the start	(lowest address) block until one	of adequate size is found. */
				pxPreviousBlock = &xStart[i];
				pxBlock = xStart[i].pxNextFreeBlock;
				while((pxBlock->xBlockSize < xWantedSize) && (pxBlock->pxNextFreeBlock != NULL))
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if(pxBlock != pxEnd[i])
				{
					/* Return the memory space pointed to - jumping over the BlockLink_t structure at its start. */
					pvReturn = (void *) (((uint8_t *) pxPreviousBlock->pxNextFreeBlock) + xHeapStructSize);

					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if((pxBlock->xBlockSize - xWantedSize) > heapMINIMUM_BLOCK_SIZE)
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = (void *) (((uint8_t *) pxBlock) + xWantedSize);
						configASSERT((((size_t) pxNewBlockLink) & portBYTE_ALIGNMENT_MASK) == 0);

						/* Calculate the sizes of two blocks split from the	single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						/* Insert the new block into the list of free blocks. */
						prvInsertBlockIntoFreeList(i, pxNewBlockLink);
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					xFreeBytesRemaining[i] -= pxBlock->xBlockSize;

					if(xFreeBytesRemaining[i] < xMinimumEverFreeBytesRemaining[i])
					{
						xMinimumEverFreeBytesRemaining[i] = xFreeBytesRemaining[i];
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = NULL;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC(pvReturn, xWantedSize);
	}
	xTaskResumeAll();

	#if(configUSE_MALLOC_FAILED_HOOK == 1)
	{
		if(pvReturn == NULL)
		{
			extern void vApplicationMallocFailedHook(void);
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	configASSERT((((size_t) pvReturn) & (size_t) portBYTE_ALIGNMENT_MASK) == 0);
	return pvReturn;
}

/*-----------------------------------------------------------*/
void *multiRegionCalloc(uint32_t i, size_t nmemb, size_t xWantedSize)
{
  void *pvReturn;
  pvReturn = multiRegionMalloc(i, nmemb * xWantedSize);
  if(pvReturn != NULL)
    memset(pvReturn, 0, nmemb * xWantedSize);
  return pvReturn;
}

/*-----------------------------------------------------------*/
void *multiRegionRealloc(uint32_t i, void *pv, size_t xWantedSize)
{
  uint8_t *puc = (uint8_t *) pv;
  BlockLink_t *pxLink;
  void *pvReturn;
  uint32_t copysize;

  /* if illegal memory region -> return NULL */
  if(i >= HEAP_NUM)
    return NULL;

  /* pre pointer == NULL ? */
  if(pv == NULL)
    return multiRegionMalloc(i, xWantedSize);

  /* The memory being freed will have an BlockLink_t structure immediately before it. */
  puc -= xHeapStructSize;

  /* This casting is to keep the compiler from issuing warnings. */
  pxLink = (void *)puc;

  /* Check the block is actually allocated. */
  configASSERT((pxLink->xBlockSize & xBlockAllocatedBit) != 0);
  configASSERT(pxLink->pxNextFreeBlock == NULL);

  /* new block size == pre block size ? */
  if(xWantedSize == pxLink->xBlockSize)
    return pv;

  /* memory alloc */
  pvReturn = multiRegionMalloc(i, xWantedSize);

  /* memory allocation failed ? */
  if(pvReturn == NULL)
  {
    multiRegionFree(i, pv);
    return NULL;
  }

  /* Memory copy */
  if((pxLink->xBlockSize & xBlockAllocatedBit) != 0)
  {
    if(xWantedSize > pxLink->xBlockSize)
      copysize = xWantedSize;
    else
      copysize = pxLink->xBlockSize;
    memcpy(pvReturn, pv, copysize);
    multiRegionFree(i, pv);
  }

  return pvReturn;
}

/*-----------------------------------------------------------*/
void multiRegionFree(uint32_t i, void *pv)
{
uint8_t *puc = (uint8_t *) pv;
BlockLink_t *pxLink;

  /* if illegal memory region -> return NULL */
  if(i >= HEAP_NUM)
    return;

	if(pv != NULL)
	{
		/* The memory being freed will have an BlockLink_t structure immediately before it. */
		puc -= xHeapStructSize;

		/* This casting is to keep the compiler from issuing warnings. */
		pxLink = (void *)puc;

		/* Check the block is actually allocated. */
		configASSERT((pxLink->xBlockSize & xBlockAllocatedBit) != 0);
		configASSERT(pxLink->pxNextFreeBlock == NULL);

		if((pxLink->xBlockSize & xBlockAllocatedBit) != 0)
		{
			if(pxLink->pxNextFreeBlock == NULL)
			{
				/* The block is being returned to the heap - it is no longer allocated. */
				pxLink->xBlockSize &= ~xBlockAllocatedBit;

				vTaskSuspendAll();
				{
					/* Add this block to the list of free blocks. */
					xFreeBytesRemaining[i] += pxLink->xBlockSize;
					traceFREE(pv, pxLink->xBlockSize);
					prvInsertBlockIntoFreeList(i, ((BlockLink_t *)pxLink));
				}
        #ifdef  osCMSIS
				(void)xTaskResumeAll();
        #endif
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}

/*-----------------------------------------------------------*/
static void prvHeapInit(uint32_t i)
{
  BlockLink_t *pxFirstFreeBlock;
  uint8_t *pucAlignedHeap;
  size_t uxAddress;

  size_t xTotalHeapSize = ucHeap[i].xSizeInBytes;

  /* Ensure the heap starts on a correctly aligned boundary. */
  uxAddress = (size_t) ucHeap[i].pucStartAddress;

  if((uxAddress & portBYTE_ALIGNMENT_MASK) != 0)
  {
    uxAddress += (portBYTE_ALIGNMENT - 1);
    uxAddress &= ~((size_t) portBYTE_ALIGNMENT_MASK);
    xTotalHeapSize -= uxAddress - (size_t) ucHeap[i].pucStartAddress;
  }

  pucAlignedHeap = (uint8_t *) uxAddress;

  /* xStart is used to hold a pointer to the first item in the list of free
  blocks.  The void cast is used to prevent compiler warnings. */
  xStart[i].pxNextFreeBlock = (void *) pucAlignedHeap;
  xStart[i].xBlockSize = (size_t) 0;

  /* pxEnd is used to mark the end of the list of free blocks and is inserted
  at the end of the heap space. */
  uxAddress = ((size_t) pucAlignedHeap) + xTotalHeapSize;
  uxAddress -= xHeapStructSize;
  uxAddress &= ~((size_t) portBYTE_ALIGNMENT_MASK);
  pxEnd[i] = (void *) uxAddress;
  pxEnd[i]->xBlockSize = 0;
  pxEnd[i]->pxNextFreeBlock = NULL;

  /* To start with there is a single free block that is sized to take up the
  entire heap space, minus the space taken by pxEnd. */
  pxFirstFreeBlock = (void *) pucAlignedHeap;
  pxFirstFreeBlock->xBlockSize = uxAddress - (size_t) pxFirstFreeBlock;
  pxFirstFreeBlock->pxNextFreeBlock = pxEnd[i];

  /* Only one block exists - and it covers the entire usable heap space. */
  xMinimumEverFreeBytesRemaining[i] = pxFirstFreeBlock->xBlockSize;
  xFreeBytesRemaining[i] = pxFirstFreeBlock->xBlockSize;

  /* Work out the position of the top bit in a size_t variable. */
  xBlockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * heapBITS_PER_BYTE) - 1);
}

/*-----------------------------------------------------------*/
static void prvInsertBlockIntoFreeList(uint32_t i, BlockLink_t *pxBlockToInsert)
{
BlockLink_t *pxIterator;
uint8_t *puc;

  /* Iterate through the list until a block is found that has a higher address
  than the block being inserted. */
  for(pxIterator = &xStart[i]; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock)
  {
    /* Nothing to do here, just iterate to the right position. */
  }

  /* Do the block being inserted, and the block it is being inserted after
  make a contiguous block of memory? */
  puc = (uint8_t *) pxIterator;
  if((puc + pxIterator->xBlockSize) == (uint8_t *) pxBlockToInsert)
  {
    pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
    pxBlockToInsert = pxIterator;
  }
  else
  {
    mtCOVERAGE_TEST_MARKER();
  }

  /* Do the block being inserted, and the block it is being inserted before
  make a contiguous block of memory? */
  puc = (uint8_t *) pxBlockToInsert;
  if((puc + pxBlockToInsert->xBlockSize) == (uint8_t *) pxIterator->pxNextFreeBlock)
  {
    if(pxIterator->pxNextFreeBlock != pxEnd[i])
    {
      /* Form one big block from the two blocks. */
      pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
      pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
    }
    else
    {
      pxBlockToInsert->pxNextFreeBlock = pxEnd[i];
    }
  }
  else
  {
    pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
  }

  /* If the block being inserted plugged a gab, so was merged with the block
  before and the block after, then it's pxNextFreeBlock pointer will have
  already been set, and should not be set here as that would make it point
  to itself. */
  if(pxIterator != pxBlockToInsert)
  {
    pxIterator->pxNextFreeBlock = pxBlockToInsert;
  }
  else
  {
    mtCOVERAGE_TEST_MARKER();
  }
}

/*-----------------------------------------------------------*/
size_t multiRegionGetFreeHeapSize(uint32_t i)
{
  /* If this is the first call to malloc then the heap will require initialisation to setup the list of free blocks. */
  if(pxEnd[i] == NULL)
  {
    vTaskSuspendAll();
    prvHeapInit(i);
    xTaskResumeAll();
  }
  return xFreeBytesRemaining[i];
}

/*-----------------------------------------------------------*/
size_t multiRegionGetMinimumEverFreeHeapSize(uint32_t i)
{
  return xMinimumEverFreeBytesRemaining[i];
}

/*-----------------------------------------------------------*/
/* return value: which memory region does the pv pointer point to? */
int32_t multiRegionSearch(void * pv)
{
  void * p;
  for(uint32_t i = 0; i < HEAP_NUM; i++)
  {
    p = ucHeap[i].pucStartAddress;
    if(pv >= p && pv < (p + ucHeap[i].xSizeInBytes))
    {
      return i;
    }
  }
  return -1;
}

/*-----------------------------------------------------------*/
/* All memory region memory free */
void free(void *pv)
{
  int32_t region = multiRegionSearch(pv);
  if(region >= 0)
    multiRegionFree(region, pv);
}

/*===========================================================*/
/* Freertos memory region (RTOSREGION9 */
#ifdef RTOSREGION

void *pvPortMalloc(size_t xWantedSize)
{
  return malloc(xWantedSize);
}

/*-----------------------------------------------------------*/
void vPortFree(void *pv)
{

  free(pv);
}

/*-----------------------------------------------------------*/
size_t xPortGetFreeHeapSize(void)
{
  return heapsize();
}

/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize(void)
{
  return xMinimumEverFreeBytesRemaining[0];
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks(void)
{
	/* This just exists to keep the linker quiet. */
}

#endif

/*===========================================================*/
/* Default memory region(s) (MALLOC_REGION) */
#ifdef MALLOC_REGION

void *malloc(size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_0(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_1(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_2(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_3(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_4(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_5(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_6(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_REGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_7(MALLOC_REGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *calloc(size_t nmemb, size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_0(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_1(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_2(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_3(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_4(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_5(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_6(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_REGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_7(MALLOC_REGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *realloc(void *pv, size_t xWantedSize)
{
  int32_t region;
  if(pv == NULL)
    return malloc(xWantedSize);
  else
  {
    region = multiRegionSearch(pv);
    if(region >= 0)
      return multiRegionRealloc(region, pv, xWantedSize);
    else
      return NULL;
  }
}

/*-----------------------------------------------------------*/
size_t heapsize(void)
{
  size_t szReturn = 0;
  #if DEFPARAM_0(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_0(MALLOC_REGION));
  #endif
  #if DEFPARAM_1(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_1(MALLOC_REGION));
  #endif
  #if DEFPARAM_2(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_2(MALLOC_REGION));
  #endif
  #if DEFPARAM_3(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_3(MALLOC_REGION));
  #endif
  #if DEFPARAM_4(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_4(MALLOC_REGION));
  #endif
  #if DEFPARAM_5(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_5(MALLOC_REGION));
  #endif
  #if DEFPARAM_6(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_6(MALLOC_REGION));
  #endif
  #if DEFPARAM_7(MALLOC_REGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_7(MALLOC_REGION));
  #endif
  return szReturn;
}

#endif  /* #ifdef MALLOC_REGION */

/*===========================================================*/
/* DMA capable memory region(s) (MALLOC_DMAREGION) */
#ifdef MALLOC_DMAREGION

void *malloc_dma(size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_0(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_1(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_2(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_3(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_4(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_5MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_6(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_7(MALLOC_DMAREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *calloc_dma(size_t nmemb, size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_0(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_1(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_2(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_3(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_4(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_5(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_6(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_DMAREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_7(MALLOC_DMAREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *realloc_dma(void *pv, size_t xWantedSize)
{
  int32_t region;
  if(pv == NULL)
    return malloc_dma(xWantedSize);
  else
  {
    region = multiRegionSearch(pv);
    if(region >= 0)
      return multiRegionRealloc(region, pv, xWantedSize);
    else
      return NULL;
  }
}

/*-----------------------------------------------------------*/
size_t heapsize_dma(void)
{
  size_t szReturn = 0;
  #if DEFPARAM_0(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_0(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_1(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_1(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_2(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_2(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_3(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_3(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_4(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_4(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_5(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_5(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_6(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_6(MALLOC_DMAREGION));
  #endif
  #if DEFPARAM_7(MALLOC_DMAREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_7(MALLOC_DMAREGION));
  #endif
  return szReturn;
}

#endif  /* #ifdef MALLOC_DMAREGION */

/*===========================================================*/
/* Internal memory region(s) (MALLOC_INTREGION) */
#ifdef MALLOC_INTREGION

void *malloc_int(size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_0(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_1(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_2(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_3(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_4(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_5MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_6(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_7(MALLOC_INTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *calloc_int(size_t nmemb, size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_0(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_1(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_2(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_3(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_4(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_5(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_6(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_INTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_7(MALLOC_INTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *realloc_int(void *pv, size_t xWantedSize)
{
  int32_t region;
  if(pv == NULL)
    return malloc_int(xWantedSize);
  else
  {
    region = multiRegionSearch(pv);
    if(region >= 0)
      return multiRegionRealloc(region, pv, xWantedSize);
    else
      return NULL;
  }
}

/*-----------------------------------------------------------*/
size_t heapsize_int(void)
{
  size_t szReturn = 0;
  #if DEFPARAM_0(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_0(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_1(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_1(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_2(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_2(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_3(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_3(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_4(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_4(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_5(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_5(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_6(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_6(MALLOC_INTREGION));
  #endif
  #if DEFPARAM_7(MALLOC_INTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_7(MALLOC_INTREGION));
  #endif
  return szReturn;
}

#endif  /* #ifdef MALLOC_INTREGION */

/*===========================================================*/
/* External memory region(s) (MALLOC_EXTREGION) */
#ifdef MALLOC_EXTREGION

void *malloc_ext(size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_0(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_1(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_2(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_3(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_4(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_5MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_6(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionMalloc(DEFPARAM_7(MALLOC_EXTREGION), xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *calloc_ext(size_t nmemb, size_t xWantedSize)
{
  void *pvReturn;
  #if DEFPARAM_0(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_0(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_1(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_1(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_2(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_2(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_3(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_3(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_4(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_4(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_5(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_5(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_6(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_6(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  #if DEFPARAM_7(MALLOC_EXTREGION) >= 0
  pvReturn = multiRegionCalloc(DEFPARAM_7(MALLOC_EXTREGION), nmemb, xWantedSize);
  if(pvReturn != NULL)
    return pvReturn;
  #endif
  return NULL;
}

/*-----------------------------------------------------------*/
void *realloc_ext(void *pv, size_t xWantedSize)
{
  int32_t region;
  if(pv == NULL)
    return malloc_ext(xWantedSize);
  else
  {
    region = multiRegionSearch(pv);
    if(region >= 0)
      return multiRegionRealloc(region, pv, xWantedSize);
    else
      return NULL;
  }
}

/*-----------------------------------------------------------*/
size_t heapsize_ext(void)
{
  size_t szReturn = 0;
  #if DEFPARAM_0(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_0(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_1(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_1(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_2(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_2(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_3(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_3(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_4(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_4(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_5(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_5(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_6(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_6(MALLOC_EXTREGION));
  #endif
  #if DEFPARAM_7(MALLOC_EXTREGION) >= 0
  szReturn += multiRegionGetFreeHeapSize(DEFPARAM_7(MALLOC_EXTREGION));
  #endif
  return szReturn;
}

#endif  /* #ifdef MALLOC_EXTREGION */
