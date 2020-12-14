//----------------------------------------------------------------------------
#if RXBUFX_SIZE < 4
#ifdef  UARTX_RX
#undef  UARTX_RX
#endif
#define UARTX_RX  X, 0, 0
#endif

#if TXBUFX_SIZE < 4
#ifdef  UARTX_TX
#undef  UARTX_TX
#endif
#define UARTX_TX  X, 0, 0
#endif

#if GPIOX_PORTNUM(UARTX_RX) >= GPIOX_PORTNUM_A
struct bufx_r {
  unsigned int in;                      /* Next In Index */
  unsigned int out;                     /* Next Out Index */
  char buf [RXBUFX_SIZE];               /* Buffer */
};
volatile static struct bufx_r rbufx = { 0, 0, };
#define FIFO_RBUFLEN ((unsigned int)(rbufx.in - rbufx.out))
__weak void uartx_cbrx(char rxch) { }
__weak void uartx_cbrxof(void)  { }
#endif

#if GPIOX_PORTNUM(UARTX_TX) >= GPIOX_PORTNUM_A
static volatile unsigned int txx_restart = 1; /* NZ if TX restart is required */
struct bufx_t {
  unsigned int in;                      /* Next In Index */
  unsigned int out;                     /* Next Out Index */
  char buf [TXBUFX_SIZE];               /* Buffer */
};
volatile static struct bufx_t tbufx = { 0, 0, };
#define FIFO_TBUFLEN ((unsigned int)(tbufx.in - tbufx.out))
#endif

void uartx_init(void);
static unsigned int uartx_inited = 0;  /* 0: not intit (call the uart_init), 1: after init */


/*----------------------------------------------------------------------------
  USARTX_IRQHandler
  Handles USARTX global interrupt request.
 *----------------------------------------------------------------------------*/
void UARTX_IRQHandler(void)
{
  unsigned int usr;

  usr = UARTX->SR;

  #if GPIOX_PORTNUM(UARTX_RX) >= GPIOX_PORTNUM_A
  unsigned int udr;
  if (usr & USART_SR_RXNE)
  {                                     /* RX */
    udr = UARTX->DR;
    uartx_cbrx((unsigned char)udr);
    if(!(usr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE)))
    {
      if (((rbufx.in - rbufx.out) & ~(RXBUFX_SIZE - 1)) == 0)
      {
        rbufx.buf [rbufx.in & (RXBUFX_SIZE - 1)] = (char)udr;
        rbufx.in++;
      }
      else
        uartx_cbrxof();                 /* buffer overflow! */
    }
    else
      /* UARTX->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF */ ;
  }
  #endif

  #if GPIOX_PORTNUM(UARTX_TX) >= GPIOX_PORTNUM_A
  if (usr & USART_SR_TXE)
  {                                     /* TX */
    if (tbufx.in != tbufx.out)
    {
      UARTX->DR = tbufx.buf [tbufx.out & (TXBUFX_SIZE - 1)] & 0x00FF;
      tbufx.out++;
      txx_restart = 0;
    }
    else
    {
      txx_restart = 1;
      UARTX->CR1 &= ~USART_CR1_TXEIE;   /* disable TX interrupt if nothing to send */
    }
  }
  #endif
}

/*------------------------------------------------------------------------------
  receive a character (if buffer is empty: return -1)
 *------------------------------------------------------------------------------*/
#if GPIOX_PORTNUM(UARTX_RX) >= GPIOX_PORTNUM_A
char uartx_getchar(char * c)
{
  if(!uartx_inited)
  {
    uartx_init();
    uartx_inited = 1;
  }

  if (rbufx.in == rbufx.out)
    return 0;                           /* empty */

  *c = rbufx.buf[rbufx.out & (RXBUFX_SIZE - 1)];
  rbufx.out++;
  return 1;
}
#else
char uartx_getchar(char * c) { return 0; }
#endif

/*------------------------------------------------------------------------------
  transmit a character
 *------------------------------------------------------------------------------*/
#if GPIOX_PORTNUM(UARTX_TX) >= GPIOX_PORTNUM_A
char uartx_sendchar(char c)
{
  if(!uartx_inited)
  {
    uartx_init();
    uartx_inited = 1;
  }

  while(FIFO_TBUFLEN >= TXBUFX_SIZE);

  tbufx.buf[tbufx.in & (TXBUFX_SIZE - 1)] = c; /* Add data to the transmit buffer */
  tbufx.in++;

  if (txx_restart)
  {                                     /* If transmit interrupt is disabled, enable it */
    txx_restart = 0;
    UARTX->CR1 |= USART_CR1_TXEIE;      /* enable TX interrupt */
  }

  return (0);
}
#else
char uartx_sendchar(char c) { return 0; }
#endif

/*------------------------------------------------------------------------------
  initialize the buffers
 *------------------------------------------------------------------------------*/
void uartx_init(void)
{
  UARTX_CLOCLK_ON;

  #if GPIOX_PORTNUM(UARTX_RX) >= GPIOX_PORTNUM_A && GPIOX_PORTNUM(UARTX_TX) >= GPIOX_PORTNUM_A
  #define UARTX_CR1_RXNEIE       USART_CR1_RXNEIE
  #define UARTX_CR1_RE           USART_CR1_RE
  #define UARTX_CR1_TE           USART_CR1_TE
  RCC->AHB1ENR |= GPIOX_CLOCK(UARTX_RX) | GPIOX_CLOCK(UARTX_TX);
  GPIOX_MODER(MODE_ALTER, UARTX_RX);
  GPIOX_MODER(MODE_ALTER, UARTX_TX);
  GPIOX_AFR(UARTX_RX);
  GPIOX_AFR(UARTX_TX);
  #elif GPIOX_PORTNUM(UARTX_RX) >= GPIOX_PORTNUM_A
  #define UARTX_CR1_RXNEIE       USART_CR1_RXNEIE
  #define UARTX_CR1_RE           USART_CR1_RE
  #define UARTX_CR1_TE           0
  RCC->AHB1ENR |= GPIOX_CLOCK(UARTX_RX);
  GPIOX_MODER(MODE_ALTER, UARTX_RX);
  GPIOX_AFR(UARTX_RX);
  #else
  #define UARTX_CR1_RXNEIE       0
  #define UARTX_CR1_RE           0
  #define UARTX_CR1_TE           USART_CR1_TE
  RCC->AHB1ENR |= GPIOX_CLOCK(UARTX_TX);
  GPIOX_MODER(MODE_ALTER, UARTX_TX);
  GPIOX_AFR(UARTX_TX);
  #endif

  /* Enable the USARTx Interrupt */
  NVIC->ISER[(((uint32_t)(int32_t)UARTX_IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)(int32_t)UARTX_IRQn) & 0x1FUL));
  NVIC->IP[((uint32_t)(int32_t)UARTX_IRQn)] = (uint8_t)((UART_PRIORITY << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);

  UARTX->CR1 = UARTX_CR1_RXNEIE | UARTX_CR1_TE | UARTX_CR1_RE | USART_CR1_PEIE;
  UARTX->BRR = UARTX_BRR_CALC;
  UARTX->CR1 |= USART_CR1_UE;
  #undef UARTX_CR1_RXNEIE
  #undef UARTX_CR1_RE
  #undef UARTX_CR1_TE
}

/*------------------------------------------------------------------------------
  printf redirect
 *------------------------------------------------------------------------------*/
#if UARTX_PRINTF == 1 && GPIOX_PORTNUM(UARTX_TX) >= GPIOX_PORTNUM_A
int _write (int fd, char *ptr, int len)
{
  int i = 0;
  while (*ptr && (i < len))
  {
    uartx_sendchar(*ptr);
    i++;
    ptr++;
  }
  return i;
}
#endif

#undef  UARTX
#undef  UARTX_IRQHandler
#undef  UARTX_IRQn
#undef  UARTX_CLOCLK_ON
#undef  UARTX_BRR_CALC
#undef  UARTX_RX
#undef  UARTX_TX
#undef  RXBUFX_SIZE
#undef  TXBUFX_SIZE
#undef  UARTX_PRINTF
#undef  uartx_inited
#undef  txx_restart
#undef  bufx_r
#undef  bufx_t
#undef  rbufx
#undef  tbufx
#undef  uartx_init
#undef  uartx_sendchar
#undef  uartx_getchar
#undef  uartx_cbrx
#undef  uartx_cbrxof
