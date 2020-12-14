/* Uart RX and TX driver for stm32f4xx
     author  : Roberto Benjami
     version : 2020.11.13
*/

#include <stdio.h>
#include "main.h"
#include "uart.h"

//----------------------------------------------------------------------------
/* GPIO mode */

/* values for GPIOX_MODER (io mode) */
#define MODE_DIGITAL_INPUT    0x0
#define MODE_OUT              0x1
#define MODE_ALTER            0x2
#define MODE_ANALOG_INPUT     0x3

/* values for GPIOX_OSPEEDR (output speed) */
#define MODE_SPD_LOW          0x0
#define MODE_SPD_MEDIUM       0x1
#define MODE_SPD_HIGH         0x2
#define MODE_SPD_VHIGH        0x3

/* values for GPIOX_OTYPER (output type: PP = push-pull, OD = open-drain) */
#define MODE_OT_PP            0x0
#define MODE_OT_OD            0x1

/* values for GPIOX_PUPDR (push up and down resistor) */
#define MODE_PU_NONE          0x0
#define MODE_PU_UP            0x1
#define MODE_PU_DOWN          0x2

#define GPIOX_(a,b,c)         GPIO ## a
#define GPIOX(a)              GPIOX_(a)

#define GPIOX_PIN_(a,b,c)     b
#define GPIOX_PIN(a)          GPIOX_PIN_(a)

#define GPIOX_AFR_(a,b,c)     GPIO ## a->AFR[b >> 3] = (GPIO ## a->AFR[b >> 3] & ~(0x0F << (4 * (b & 7)))) | (c << (4 * (b & 7)));
#define GPIOX_AFR(a)          GPIOX_AFR_(a)

#define GPIOX_MODER_(a,b,c,d) GPIO ## b->MODER = (GPIO ## b->MODER & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_MODER(a, b)     GPIOX_MODER_(a, b)

#define GPIOX_OTYPER_(a,b,c,d) GPIO ## b->OTYPER = (GPIO ## b->OTYPER & ~(1 << c)) | (a << c);
#define GPIOX_OTYPER(a, b)    GPIOX_OTYPER_(a, b)

#define GPIOX_OSPEEDR_(a,b,c,d) GPIO ## b->OSPEEDR = (GPIO ## b->OSPEEDR & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_OSPEEDR(a, b)   GPIOX_OSPEEDR_(a, b)

#define GPIOX_PUPDR_(a,b,c,d) GPIO ## b->PUPDR = (GPIO ## b->PUPDR & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_PUPDR(a, b)     GPIOX_PUPDR_(a, b)

#define GPIOX_SET_(a,b,c)     GPIO ## a ->BSRR = 1 << b
#define GPIOX_SET(a)          GPIOX_SET_(a)

#define GPIOX_CLR_(a,b,c)     GPIO ## a ->BSRR = 1 << (b + 16)
#define GPIOX_CLR(a)          GPIOX_CLR_(a)

#define GPIOX_IDR_(a,b,c)     (GPIO ## a ->IDR & (1 << b))
#define GPIOX_IDR(a)          GPIOX_IDR_(a)

#define GPIOX_LINE_(a,b,c)    EXTI_Line ## b
#define GPIOX_LINE(a)         GPIOX_LINE_(a)

#define GPIOX_PORTSRC_(a,b,c) GPIO_PortSourceGPIO ## a
#define GPIOX_PORTSRC(a)      GPIOX_PORTSRC_(a)

#define GPIOX_PINSRC_(a,b,c)  GPIO_PinSource ## b
#define GPIOX_PINSRC(a)       GPIOX_PINSRC_(a)

#define GPIOX_CLOCK_(a,b,c)   RCC_AHB1ENR_GPIO ## a ## EN
#define GPIOX_CLOCK(a)        GPIOX_CLOCK_(a)

#define GPIOX_PORTNUM_A       1
#define GPIOX_PORTNUM_B       2
#define GPIOX_PORTNUM_C       3
#define GPIOX_PORTNUM_D       4
#define GPIOX_PORTNUM_E       5
#define GPIOX_PORTNUM_F       6
#define GPIOX_PORTNUM_G       7
#define GPIOX_PORTNUM_H       8
#define GPIOX_PORTNUM_I       9
#define GPIOX_PORTNUM_J       10
#define GPIOX_PORTNUM_K       11
#define GPIOX_PORTNUM_(a,b,c) GPIOX_PORTNUM_ ## a
#define GPIOX_PORTNUM(a)      GPIOX_PORTNUM_(a)

#define GPIOX_PORTNAME_(a,b,c) a
#define GPIOX_PORTNAME(a)     GPIOX_PORTNAME_(a)

//----------------------------------------------------------------------------
#if UART1_BAUDRATE > 0 && (GPIOX_PORTNUM(UART1_RX) >= GPIOX_PORTNUM_A && RXBUF1_SIZE >= 4 || GPIOX_PORTNUM(UART1_TX) >= GPIOX_PORTNUM_A && TXBUF1_SIZE >= 4)
#define UARTX                 USART1
#define UARTX_IRQHandler      USART1_IRQHandler
#define UARTX_IRQn            USART1_IRQn
#define UARTX_CLOCLK_ON       RCC->APB2ENR |= RCC_APB2ENR_USART1EN
#define UARTX_BRR_CALC        (UART_1_6_CLK) / UART1_BAUDRATE
#define UARTX_RX              UART1_RX
#define UARTX_TX              UART1_TX
#define TXBUFX_SIZE           TXBUF1_SIZE
#define RXBUFX_SIZE           RXBUF1_SIZE
#define UARTX_PRINTF          UART1_PRINTF
#define uartx_inited          uart1_inited
#define txx_restart           tx1_restart
#define bufx_r                buf1_r
#define bufx_t                buf1_t
#define rbufx                 rbuf1
#define tbufx                 tbuf1
#define uartx_init            uart1_init
#define uartx_sendchar        uart1_sendchar
#define uartx_getchar         uart1_getchar
#define uartx_cbrx            uart1_cbrx
#define uartx_cbrxof          uart1_cbrxof
#include "uartx.h"
#endif

#if UART2_BAUDRATE > 0 && (GPIOX_PORTNUM(UART2_RX) >= GPIOX_PORTNUM_A && RXBUF2_SIZE >= 4 || GPIOX_PORTNUM(UART2_TX) >= GPIOX_PORTNUM_A && TXBUF2_SIZE >= 4)
#define UARTX                 USART2
#define UARTX_IRQHandler      USART2_IRQHandler
#define UARTX_IRQn            USART2_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1ENR |= RCC_APB1ENR_USART2EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART2_BAUDRATE
#define UARTX_RX              UART2_RX
#define UARTX_TX              UART2_TX
#define TXBUFX_SIZE           TXBUF2_SIZE
#define RXBUFX_SIZE           RXBUF2_SIZE
#define UARTX_PRINTF          UART2_PRINTF
#define uartx_inited          uart2_inited
#define txx_restart           tx2_restart
#define bufx_r                buf2_r
#define bufx_t                buf2_t
#define rbufx                 rbuf2
#define tbufx                 tbuf2
#define uartx_init            uart2_init
#define uartx_sendchar        uart2_sendchar
#define uartx_getchar         uart2_getchar
#define uartx_cbrx            uart2_cbrx
#define uartx_cbrxof          uart2_cbrxof
#include "uartx.h"
#endif

#if UART3_BAUDRATE > 0 && (GPIOX_PORTNUM(UART3_RX) >= GPIOX_PORTNUM_A && RXBUF3_SIZE >= 4 || GPIOX_PORTNUM(UART3_TX) >= GPIOX_PORTNUM_A && TXBUF3_SIZE >= 4)
#define UARTX                 USART3
#define UARTX_IRQHandler      USART3_IRQHandler
#define UARTX_IRQn            USART3_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1ENR |= RCC_APB1ENR_USART3EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART3_BAUDRATE
#define UARTX_RX              UART3_RX
#define UARTX_TX              UART3_TX
#define TXBUFX_SIZE           TXBUF3_SIZE
#define RXBUFX_SIZE           RXBUF3_SIZE
#define UARTX_PRINTF          UART3_PRINTF
#define uartx_inited          uart3_inited
#define txx_restart           tx3_restart
#define bufx_r                buf3_r
#define bufx_t                buf3_t
#define rbufx                 rbuf3
#define tbufx                 tbuf3
#define uartx_init            uart3_init
#define uartx_sendchar        uart3_sendchar
#define uartx_getchar         uart3_getchar
#define uartx_cbrx            uart3_cbrx
#define uartx_cbrxof          uart3_cbrxof
#include "uartx.h"
#endif

#if UART4_BAUDRATE > 0 && (GPIOX_PORTNUM(UART4_RX) >= GPIOX_PORTNUM_A && RXBUF4_SIZE >= 4 || GPIOX_PORTNUM(UART4_TX) >= GPIOX_PORTNUM_A && TXBUF4_SIZE >= 4)
#define UARTX                 UART4
#define UARTX_IRQHandler      UART4_IRQHandler
#define UARTX_IRQn            UART4_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1ENR |= RCC_APB1ENR_UART4EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART4_BAUDRATE
#define UARTX_RX              UART4_RX
#define UARTX_TX              UART4_TX
#define TXBUFX_SIZE           TXBUF4_SIZE
#define RXBUFX_SIZE           RXBUF4_SIZE
#define UARTX_PRINTF          UART4_PRINTF
#define uartx_inited          uart4_inited
#define txx_restart           tx4_restart
#define bufx_r                buf4_r
#define bufx_t                buf4_t
#define rbufx                 rbuf4
#define tbufx                 tbuf4
#define uartx_init            uart4_init
#define uartx_sendchar        uart4_sendchar
#define uartx_getchar         uart4_getchar
#define uartx_cbrx            uart4_cbrx
#define uartx_cbrxof          uart4_cbrxof
#include "uartx.h"
#endif

#if UART5_BAUDRATE > 0 && (GPIOX_PORTNUM(UART5_RX) >= GPIOX_PORTNUM_A && RXBUF5_SIZE >= 4 || GPIOX_PORTNUM(UART5_TX) >= GPIOX_PORTNUM_A && TXBUF5_SIZE >= 4)
#define UARTX                 UART5
#define UARTX_IRQHandler      UART5_IRQHandler
#define UARTX_IRQn            UART5_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1ENR |= RCC_APB1ENR_UART5EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART5_BAUDRATE
#define UARTX_RX              UART5_RX
#define UARTX_TX              UART5_TX
#define TXBUFX_SIZE           TXBUF5_SIZE
#define RXBUFX_SIZE           RXBUF5_SIZE
#define UARTX_PRINTF          UART5_PRINTF
#define uartx_inited          uart5_inited
#define txx_restart           tx5_restart
#define bufx_r                buf5_r
#define bufx_t                buf5_t
#define rbufx                 rbuf5
#define tbufx                 tbuf5
#define uartx_init            uart5_init
#define uartx_sendchar        uart5_sendchar
#define uartx_getchar         uart5_getchar
#define uartx_cbrx            uart5_cbrx
#define uartx_cbrxof          uart5_cbrxof
#include "uartx.h"
#endif

#if UART6_BAUDRATE > 0 && (GPIOX_PORTNUM(UART6_RX) >= GPIOX_PORTNUM_A && RXBUF6_SIZE >= 4 || GPIOX_PORTNUM(UART6_TX) >= GPIOX_PORTNUM_A && TXBUF6_SIZE >= 4)
#define UARTX                 USART6
#define UARTX_IRQHandler      USART6_IRQHandler
#define UARTX_IRQn            USART6_IRQn
#define UARTX_CLOCLK_ON       RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
#define UARTX_BRR_CALC        (UART_1_6_CLK) / UART6_BAUDRATE
#define UARTX_RX              UART6_RX
#define UARTX_TX              UART6_TX
#define TXBUFX_SIZE           TXBUF6_SIZE
#define RXBUFX_SIZE           RXBUF6_SIZE
#define UARTX_PRINTF          UART6_PRINTF
#define uartx_inited          uart6_inited
#define txx_restart           tx6_restart
#define bufx_r                buf6_r
#define bufx_t                buf6_t
#define rbufx                 rbuf6
#define tbufx                 tbuf6
#define uartx_init            uart6_init
#define uartx_sendchar        uart6_sendchar
#define uartx_getchar         uart6_getchar
#define uartx_cbrx            uart6_cbrx
#define uartx_cbrxof          uart6_cbrxof
#include "uartx.h"
#endif

#if UART7_BAUDRATE > 0 && (GPIOX_PORTNUM(UART7_RX) >= GPIOX_PORTNUM_A && RXBUF7_SIZE >= 4 || GPIOX_PORTNUM(UART7_TX) >= GPIOX_PORTNUM_A && TXBUF7_SIZE >= 4)
#define UARTX                 UART7
#define UARTX_IRQHandler      UART7_IRQHandler
#define UARTX_IRQn            UART7_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1ENR |= RCC_APB1ENR_UART7EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART7_BAUDRATE
#define UARTX_RX              UART7_RX
#define UARTX_TX              UART7_TX
#define TXBUFX_SIZE           TXBUF7_SIZE
#define RXBUFX_SIZE           RXBUF7_SIZE
#define UARTX_PRINTF          UART7_PRINTF
#define uartx_inited          uart7_inited
#define txx_restart           tx7_restart
#define bufx_r                buf7_r
#define bufx_t                buf7_t
#define rbufx                 rbuf7
#define tbufx                 tbuf7
#define uartx_init            uart7_init
#define uartx_sendchar        uart7_sendchar
#define uartx_getchar         uart7_getchar
#define uartx_cbrx            uart7_cbrx
#define uartx_cbrxof          uart7_cbrxof
#include "uartx.h"
#endif

#if UART8_BAUDRATE > 0 && (GPIOX_PORTNUM(UART8_RX) >= GPIOX_PORTNUM_A && RXBUF8_SIZE >= 4 || GPIOX_PORTNUM(UART8_TX) >= GPIOX_PORTNUM_A && TXBUF8_SIZE >= 4)
#define UARTX                 UART8
#define UARTX_IRQHandler      UART8_IRQHandler
#define UARTX_IRQn            UART8_IRQn
#define UARTX_CLOCLK_ON       RCC->APB1LENR |= RCC_APB1LENR_UART8EN
#define UARTX_BRR_CALC        (UART_2_3_4_5_7_8_CLK) / UART8_BAUDRATE
#define UARTX_RX              UART8_RX
#define UARTX_TX              UART8_TX
#define TXBUFX_SIZE           TXBUF8_SIZE
#define RXBUFX_SIZE           RXBUF8_SIZE
#define UARTX_PRINTF          UART8_PRINTF
#define uartx_inited          uart8_inited
#define txx_restart           tx8_restart
#define bufx_r                buf8_r
#define bufx_t                buf8_t
#define rbufx                 rbuf8
#define tbufx                 tbuf8
#define uartx_init            uart8_init
#define uartx_sendchar        uart8_sendchar
#define uartx_getchar         uart8_getchar
#define uartx_cbrx            uart8_cbrx
#define uartx_cbrxof          uart8_cbrxof
#include "uartx.h"
#endif
