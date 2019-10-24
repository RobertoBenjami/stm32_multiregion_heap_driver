/* Multi region memory allocation test (multi_heap_4)
 *
 * készitö: Roberto Benjami
 * verzio:  2019.10
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "main.h"

#include "multi_heap_4.h"

#define POINTERS_NUM          512     // number of blocks
#define IMALLOC_SIZE          1024    // internal sram block size
#define CMALLOC_SIZE          1024    // ccm sram block size
#define EMALLOC_SIZE          128000  // external ram block size

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
    p[i] = iramMalloc(IMALLOC_SIZE);
    printf("i ram malloc:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)iramGetFreeHeapSize());
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
      iramFree(p[i]);
      printf("i ram free:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)iramGetFreeHeapSize());
      Delay(PRINTF_DELAY);
    }
    i++;
  } while(i < POINTERS_NUM);

  i = 0;
  do
  {
    p[i] = cramMalloc(CMALLOC_SIZE);
    printf("ccm ram malloc:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)cramGetFreeHeapSize());
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
      cramFree(p[i]);
      printf("ccm ram free:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)cramGetFreeHeapSize());
      Delay(PRINTF_DELAY);
    }
    i++;
  } while(i < POINTERS_NUM);

  i = 0;
  do
  {
    p[i] = eramMalloc(EMALLOC_SIZE);
    printf("e ram malloc:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)eramGetFreeHeapSize());
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
      eramFree(p[i]);
      printf("i ram free:p%d:0x%x, 0x%x\r\n", (unsigned int)i, (unsigned int)p[i], (unsigned int)eramGetFreeHeapSize());
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
