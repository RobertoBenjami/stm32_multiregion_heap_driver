/*
multi_heap.h

Created on: dec 14, 2020
    Author: Benjami

It is a multi-region memory management features abilities:
- manage multiple memory regions (up to 8 regions)
- freertos compatibility function name (pvPortMalloc, vPortFree, xPortGetFreeHeapSize)
- management of regions with different characteristics (dma capable, internal, external memory)
- you can set the order in which each region is occupied when reserving memory
- region independent free function (finds in which region the memory to be released is located)
- querying the total amount of free memory in several memory regions
*/

#ifndef __MULTI_HEAP_H_
#define __MULTI_HEAP_H_

#ifdef __cplusplus
 extern "C" {
#endif

/*-----------------------------------------------------------------------------
  Parameter section (set the parameters)
*/

/*
Setting:
- HEAP_NUM : how many heap memory region(s) (1..8)
- HEAP_REGIONS : memory address and size for regions (see the example)
- configTOTAL_HEAP_SIZE : shared (with stack and bss) memory region heap size
- RTOSREGION : which memory region does freertos use? (if you don't use freertos, you can delete it)
- MALLOC_REGION : which memory region(s) does the malloc, calloc, realloc, heapsize functions ?
                  - if you don't use this functions, you can delete it
                  - the order is also important, looking for free memory in the order listed
                  - unused regions can be marked with -1
                  - the number of region parameters must be equal to HEAP_NUM
- MALLOC_DMAREGION : which memory region(s) does the malloc_dma, calloc_dma, realloc_dma, heapsize_dma functions ?
                  - the parameterization is the same as MALLOC_REGION
- MALLOC_INTREGION : which memory region(s) does the malloc_int, calloc_int, realloc_int, heapsize_int functions ?
                  - the parameterization is the same as MALLOC_REGION
- MALLOC_EXTREGION : which memory region(s) does the malloc_ext, calloc_ext, realloc_ext, heapsize_ext functions ?
                  - the parameterization is the same as MALLOC_REGION

This example contains 1 regions (stm32f103c8t)
- 0: default 20kB ram       (0x20000000..0x20004FFF, shared with stack and bss)
  #define HEAP_NUM          1
  #define HEAP_REGIONS      {(uint8_t *) &ucHeap0, sizeof(ucHeap0)};
  #define RTOSREGION        0  // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 0
  #define MALLOC_REGION     0  // malloc, calloc, realloc, heapsize : region 0
  #define MALLOC_DMAREGION  0  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 0

This example contains 2 regions (stm32f407vet or stm32f407zet board)
- 0: default 128kB ram      (0x20000000..0x2001FFFF, shared with stack and bss)
- 1: ccm internal 64kB ram  (0x10000000..0x1000FFFF)
  #define HEAP_NUM          2
  #define HEAP_REGIONS      {{ (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                             { (uint8_t *) 0x18000000, 0x10000       }};
  #define RTOSREGION        0      // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 0
  #define MALLOC_REGION     1,  0  // malloc, calloc, realloc, heapsize : region 1 + region 0
  #define MALLOC_DMAREGION  0, -1  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 0

This example contains 3 regions (stm32f407zet board with external 1MB sram chip with FSMC)
- 0: default 128kB ram      (0x20000000..0x2001FFFF, shared with stack and bss)
- 1: ccm internal 64kB ram  (0x10000000..0x1000FFFF)
- 2: external 1MB sram      (0x68000000..0x680FFFFF)
  #define HEAP_NUM          3
  #define HEAP_REGIONS      {{ (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                             { (uint8_t *) 0x10000000, 0x10000       },\
                             { (uint8_t *) 0x68000000, 0x100000      }};
  #define RTOSREGION        0          // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 0
  #define MALLOC_REGION     2,  1,  0  // malloc, calloc, realloc, heapsize : region 2 + region 1 + region 0
  #define MALLOC_DMAREGION  0, -1, -1  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 0
  #define MALLOC_INTREGION  1,  0, -1  // malloc_int, calloc_int, realloc_int, heapsize_int : region 1 + region 0
  #define MALLOC_EXTREGION  2, -1, -1  // malloc_ext, calloc_ext, realloc_ext, heapsize_ext : region 2

This example contains 3 regions (stm32f429zit discovery with external 8MB sdram chip with FMC)
- 0: default 192kB ram      (0x20000000..0x2002FFFF, shared with stack and bss)
- 1: ccm internal 64kB ram  (0x10000000..0x1000FFFF)
- 2: external 8MB sdram     (0xD0000000..0xD07FFFFF)
  #define HEAP_NUM          3
  #define HEAP_REGIONS      {{ (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                             { (uint8_t *) 0x10000000, 0x10000       },\
                             { (uint8_t *) 0xD0000000, 0x800000      }};
  #define RTOSREGION        0          // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 0
  #define MALLOC_REGION     2,  1,  0  // malloc, calloc, realloc, heapsize : region 2 + region 1 + region 0
  #define MALLOC_DMAREGION  0, -1, -1  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 0
  #define MALLOC_INTREGION  1,  0, -1  // malloc_int, calloc_int, realloc_int, heapsize_int : region 1 + region 0
  #define MALLOC_EXTREGION  2, -1, -1  // malloc_ext, calloc_ext, realloc_ext, heapsize_ext : region 2

This example contains 5 regions (stm32h743vit board)
- 0: ITC internal 64kB      (0x00000000..0x0000FFFF)
- 1: DTC internal 128kB     (0x20000000..0x2001FFFF)
- 2: D1 internal 512kB ram  (0x24000000..0x2407FFFF, shared with stack and bss)
- 3: D2 internal 288kB ram  (0x30000000..0x30047FFF)
- 4: D3 internal 64kB ram   (0x38000000..0x3800FFFF)
  #define HEAP_NUM          5
  #define HEAP_REGIONS      {{ (uint8_t *) 0x00000000, 0x10000       },\
                             { (uint8_t *) 0x20000000, 0x20000       },\
                             { (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                             { (uint8_t *) 0x30000000, 0x48000       },\
                             { (uint8_t *) 0x38000000, 0x10000       }};
  #define RTOSREGION        2                  // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 2
  #define MALLOC_REGION     0,  1,  4,  3,  2  // malloc, calloc, realloc, heapsize : region 0 + region 1 + region 4 + region 3 + region 2
  #define MALLOC_DMAREGION  4,  3,  2, -1, -1  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 4 + region 3 + region 2

This example contains 6 regions (stm32h743zit for external 8MB sdram chip with FMC)
- 0: ITC internal 64kB      (0x00000000..0x0000FFFF)
- 1: DTC internal 128kB     (0x20000000..0x2001FFFF)
- 2: D1 internal 512kB ram  (0x24000000..0x2407FFFF, shared with stack and bss)
- 3: D2 internal 288kB ram  (0x30000000..0x30047FFF)
- 4: D3 internal 64kB ram   (0x38000000..0x3800FFFF)
- 5: external 8MB sdram     (0xD0000000..0xD07FFFFF)
  #define HEAP_NUM          6
  #define HEAP_REGIONS      {{ (uint8_t *) 0x00000000, 0x10000       },\
                             { (uint8_t *) 0x20000000, 0x20000       },\
                             { (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                             { (uint8_t *) 0x30000000, 0x48000       },\
                             { (uint8_t *) 0x38000000, 0x10000       },\
                             { (uint8_t *) 0xD0000000, 0x800000      }};
  #define RTOSREGION        2                      // pvPortMalloc, vPortFree, xPortGetFreeHeapSize : region 2
  #define MALLOC_REGION     0,  1,  4,  3,  2,  5  // malloc, calloc, realloc, heapsize : region 0 + region 1 + region 4 + region 3 + region 2 + region 5
  #define MALLOC_DMAREGION  4,  3,  2, -1, -1, -1  // malloc_dma, calloc_dma, realloc_dma, heapsize_dma : region 4 + region 3 + region 2
  #define MALLOC_INTREGION  0,  1,  4,  3,  2, -1  // malloc_int, calloc_int, realloc_int, heapsize_int : region 0 + region 1 + region 4 + region 3 + region 2
  #define MALLOC_EXTREGION  5, -1, -1, -1, -1, -1  // malloc_ext, calloc_ext, realloc_ext, heapsize_ext : region 5
*/

/* Heap region number (1..8) */
#define HEAP_NUM      3

/*
Default heap region static reservation, here it has to share with bss and stack (if not defined in freertos)
The setting method:
- step 1: setting the 8 value
- step 2: compile the program
- step 3: check the freee memory in shared region
- step 4: the value be a little less than free memory this region */
#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE    186000
#endif

#define HEAP_SHARED    ucHeap0[configTOTAL_HEAP_SIZE]  /* shared (with stack and bss) memory region heap */

/* Regions table: adress and size (internal sram region (0), ccmram region (1), external ram region (2) ) */
#define HEAP_REGIONS  {{ (uint8_t *) &ucHeap0, sizeof(ucHeap0) },\
                       { (uint8_t *) 0x10000000, 0x10000       },\
                       { (uint8_t *) 0xD0000000, 0x800000      }};
/*
It is possible to search and reserve free memory from several memory regions.
- pvPortMalloc, vPortFree, xPortGetFreeHeapSize : freertos memory region
- malloc, calloc, realloc, heapsize: default memory region(s)
- malloc_dma, calloc_dma, realloc_dma, heapsize_dma: dma capable memory region(s)
- malloc_int, calloc_int, realloc_int, heapsize_int: internal memory region(s)
- malloc_ext, calloc_ext, realloc_ext, heapsize_ext: external memory region(s)
  The region order must be specified in the definitions. The number of parameters in the HEAP_NUM definition is mandatory,
  unused regions can be marked with -1. */
// #define RTOSREGION        0
#define MALLOC_REGION     2,  1,  0
#define MALLOC_DMAREGION  0, -1, -1
#define MALLOC_INTREGION  1,  0, -1
#define MALLOC_EXTREGION  2, -1, -1

/* For program debug (e.g. printf the memory allocations and memory free) */
// #define traceMALLOC(pvAddress, uiSize)  printf("%x %d malloc\r\n", pvAddress, (unsigned int)uiSize)
// #define traceFREE(pvAddress, uiSize)    printf("%x %d free\r\n", pvAddress, (unsigned int)uiSize)

/*-----------------------------------------------------------------------------
  Fix section, do not change
*/

/* User definied memory region */
void    *multiRegionMalloc(uint32_t i, size_t xWantedSize);
void    *multiRegionCalloc(uint32_t i, size_t nmemb, size_t xWantedSize);
void    *multiRegionRealloc(uint32_t i, void *pv, size_t xWantedSize);
void    multiRegionFree(uint32_t i, void *pv);
size_t  multiRegionGetFreeHeapSize(uint32_t i);
size_t  multiRegionGetMinimumEverFreeHeapSize(uint32_t i);

/* Freertos memory region (RTOSREGION) */
void    *pvPortMalloc(size_t xWantedSize);
void    vPortFree(void *pv);
size_t  xPortGetFreeHeapSize(void);
size_t  xPortGetMinimumEverFreeHeapSize(void);
void    vPortInitialiseBlocks(void);

/* Default memory region(s) (MALLOC_REGION) */
void    *malloc(size_t xWantedSize);
void    *calloc(size_t nmemb, size_t xWantedSize);
void    *realloc(void *pv, size_t xWantedSize); /* if pv is valid -> new pointer region is equal previsous pointer region */
size_t  heapsize(void);

/* DMA capable memory region(s) (MALLOC_DMAREGION) */
void    *malloc_dma(size_t xWantedSize);
void    *calloc_dma(size_t nmemb, size_t xWantedSize);
void    *realloc_dma(void *pv, size_t xWantedSize); /* if pv is valid -> new pointer region is equal previsous pointer region */
size_t  heapsize_dma(void);

/* Internal memory region(s) (MALLOC_INTREGION) */
void    *malloc_int(size_t xWantedSize);
void    *calloc_int(size_t nmemb, size_t xWantedSize);
void    *realloc_int(void *pv, size_t xWantedSize); /* if pv is valid -> new pointer region is equal previsous pointer region */
size_t  heapsize_int(void);

/* External memory region(s)  (MALLOC_EXTREGION)*/
void    *malloc_ext(size_t xWantedSize);
void    *calloc_ext(size_t nmemb, size_t xWantedSize);
void    *realloc_ext(void *pv, size_t xWantedSize); /* if pv is valid -> new pointer region is equal previsous pointer region */
size_t  heapsize_ext(void);

/* Region independence free (it first finds the region) */
void    free(void *pv);

#ifdef __cplusplus
}
#endif

#endif /* __MULTI_HEAP_H_ */
