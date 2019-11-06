# STM32Fxxx multi region heap management

This is an extension of the freertos memory management source (heap_4.c) to handle more non continuous memory regions 
(this project was created because the heap_4 only one region handles, the heap_5 handles multiple regions but in one unit).
This the multi_heap_4 separately handles the all memory regions. The available memory regions can be determined in the header file (multi_heap_4.h). This memory management can also be used without freertos.

Memory regions examples and comment: https://github.com/RobertoBenjami/stm32_multiregion_heap_driver/blob/master/multiregion_heap_driver.pdf
