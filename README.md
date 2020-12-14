# STM32Fxxx multi region heap management

This is an extension of the freertos memory management source (heap_4.c) to handle more non continuous memory regions 
(I created this project because the heap_4 only one region handles, the heap_5 handles multiple regions but in one unit).
This the multi_heap separately handles the all memory regions. The available memory regions can be determined in the header file (multi_heap.h). This memory management can also be used without freertos.

The multi-region memory management features abilities:
- manage multiple memory regions (up to 8 regions)
- freertos compatibility function name (pvPortMalloc, vPortFree, xPortGetFreeHeapSize)
- management of regions with different characteristics (dma capable, internal, external memory)
- you can set the order in which each region is occupied when reserving memory
- region independent free function (finds in which region the memory to be released is located)
- querying the total amount of free memory in several memory regions
