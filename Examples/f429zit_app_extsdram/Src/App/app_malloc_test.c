/* Multi region memory allocation test (multi_heap)
 *
 * author: Roberto Benjami
 * version: 2020.12
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "main.h"

#include "multi_heap.h"

#define POINTERS_NUM          512     /* number of blocks */
#define IMALLOC_SIZE          1024    /* internal sram block size */
#define EMALLOC_SIZE          128000  /* external sdram block size */

#define PRINTF_DELAY          20

//-----------------------------------------------------------------------------
// freertos vs HAL
#ifdef  osCMSIS
#define Delay(t)              osDelay(t)
#define GetTime()             osKernelSysTick()
#else
#define Delay(t)              HAL_Delay(t)
#define GetTime()             HAL_GetTick()
#endif


//-----------------------------------------------------------------------------
#ifdef osCMSIS
void StartDefaultTask(void const * argument)
#else
void mainApp(void)
#endif
{
  static uint8_t * p[POINTERS_NUM];
  uint32_t i;
  printf("\r\nMem malloc test\r\n");
  Delay(PRINTF_DELAY);

  i = 0;
  do
  {
    p[i] = malloc_int(IMALLOC_SIZE);
    printf("int ram malloc:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)heapsize_int());
    Delay(PRINTF_DELAY);
    i++;
  } while((i < POINTERS_NUM) && p[i - 1]);
  i = 0;
  do
  {
    if(p[i] == NULL)
      break;
    else
    {
      free(p[i]);
      printf("int ram free:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)heapsize_int());
      Delay(PRINTF_DELAY);
    }
    i++;
  } while(i < POINTERS_NUM);

  i = 0;
  do
  {
    p[i] = malloc_ext(EMALLOC_SIZE);
    printf("ext ram malloc:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)heapsize_ext());
    Delay(PRINTF_DELAY);
    i++;
  } while((i < POINTERS_NUM) && p[i - 1]);
  i = 0;
  do
  {
    if(p[i] == NULL)
      break;
    else
    {
      free(p[i]);
      printf("ext ram free:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)heapsize_ext());
      Delay(PRINTF_DELAY);
    }
    i++;
  } while(i < POINTERS_NUM);

  printf("End test\r\n");

  while(1)
  {
    Delay(1000);
  }
}


//-----------------------------------------------------------------------------
#ifdef osCMSIS
void StartTask02(void const * argument)
{
  for(;;)
  {
    Delay(1);
  }
}
#endif
