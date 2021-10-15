#ifndef __PLATFORM_CONFIG_H
#define __PLATFORM_CONFIG_H

 #include "stm32f10x.h"
 
/*
    显示屏串口__串口2
*/
#define SCREEN_COM                     USART2
#define SCREEN_COM_CLK                 RCC_APB1Periph_USART2
#define SCREEN_COM_TX_PORT             GPIOA    
#define SCREEN_COM_TX_PIN              GPIO_Pin_2
#define SCREEN_COM_TX_CLK              RCC_APB2Periph_GPIOA
#define SCREEN_COM_RX_PORT             GPIOA
#define SCREEN_COM_RX_PIN              GPIO_Pin_3
#define SCREEN_COM_RX_CLK              RCC_APB2Periph_GPIOA
#define SCREEN_POWER_CTRL_PORT         GPIOA
#define SCREEN_POWER_CTRL_PIN          GPIO_Pin_1
#define SCREEN_POWER_CTRL_CLK          RCC_APB2Periph_GPIOA
#define SCREEN_COM_IRQn                USART2_IRQn
#define SCREEN_COM_TX_DMA1CHANNEL      DMA1_Channel7
#define SCREEN_COM_RX_DMA1CHANNEL      DMA1_Channel6
#define SCREEN_COM_TX_IRQn             DMA1_Channel7_IRQn
#define SCREEN_COM_RX_IRQn             DMA1_Channel6_IRQn
#define SCREEN_COM_IRQHandler          USART2_IRQHandler 

/*
    GPS串口__串口3
*/
#define GPS_COM                        USART3
#define GPS_COM_CLK                    RCC_APB1Periph_USART3
#define GPS_COM_TX_PORT                GPIOB
#define GPS_COM_TX_PIN                 GPIO_Pin_10
#define GPS_COM_TX_CLK                 RCC_APB2Periph_GPIOB
#define GPS_COM_RX_PORT                GPIOB
#define GPS_COM_RX_PIN                 GPIO_Pin_11
#define GPS_COM_RX_CLK                 RCC_APB2Periph_GPIOB
#define GPS_COM_IRQn                   USART3_IRQn       
#define GPS_COM_TX_DMA1CHANNEL         DMA1_Channel2     
#define GPS_COM_RX_DMA1CHANNEL         DMA1_Channel3     
#define GPS_COM_TX_DMA1_IRQn           DMA1_Channel2_IRQn
#define GPS_COM_RX_DMA1_IRQn           DMA1_Channel3_IRQn
#define GPS_COM_IRQHandler             USART3_IRQHandler

/*
    RS232串口__串口1
*/
#define Shell_COM                      USART1
#define Shell_COM_CLK                  RCC_APB2Periph_USART1
#define Shell_COM_TX_PORT              GPIOB               
#define Shell_COM_TX_PIN               GPIO_Pin_6          
#define Shell_COM_TX_CLK               RCC_APB2Periph_GPIOB
#define Shell_COM_RX_PORT              GPIOB               
#define Shell_COM_RX_PIN               GPIO_Pin_7//PB7         
#define Shell_COM_RX_CLK               RCC_APB2Periph_GPIOB
#define Shell_COM_IRQn                 USART1_IRQn
//#define Shell_COM_TX_DMA1CHANNEL       DMA1_Channel4
//#define Shell_COM_RX_DMA1CHANNEL       DMA1_Channel5
//#define Shell_COM_TX_DMA1_IRQn         DMA1_Channel4_IRQn
//#define Shell_COM_RX_DMA1_IRQn         DMA1_Channel5_IRQn
#define Shell_COM_IRQHandler           USART1_IRQHandler

#endif
