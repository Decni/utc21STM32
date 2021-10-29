#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "gps.h"
#include "platform_config.h"
#include "spi.h"
#include "shell.h"
#include "screen.h"

tGpsReceve  GpsReceveA, GpsReceveB;
tGpsReceve *pGpsReceve, *pGpsProcess;
bool        RTC_InitFlag;
static void DMA_Config_GPS(void);

/*
    GPS串口初始化函数
*/
void ComInit_GPS(uint32_t USART_BaudRate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef  NVIC_InitStruct;
    
    GpsReceveA.size = 0;
    GpsReceveB.size = 0;
    pGpsProcess     = (tGpsReceve*)0;
    pGpsReceve      = &GpsReceveA;
    RTC_InitFlag    = false;

    USART_DeInit(GPS_COM);
    USART_LINCmd(GPS_COM, DISABLE);
    RCC_APB2PeriphClockCmd(GPS_COM_RX_CLK | GPS_COM_TX_CLK | RCC_APB2Periph_AFIO,ENABLE);
    RCC_APB1PeriphClockCmd(GPS_COM_CLK,ENABLE);
    
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin   = GPS_COM_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPS_COM_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin  = GPS_COM_RX_PIN;
    GPIO_Init(GPS_COM_RX_PORT, &GPIO_InitStructure);
    
    DMA_Config_GPS();

    USART_InitStruct.USART_BaudRate            = USART_BaudRate;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Rx;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_Init(GPS_COM, &USART_InitStruct);
    
    NVIC_InitStruct.NVIC_IRQChannel                   = GPS_COM_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    USART_DMACmd(GPS_COM,USART_DMAReq_Rx,ENABLE);
    USART_ClearFlag(GPS_COM,USART_FLAG_IDLE);
    USART_ITConfig(GPS_COM,USART_IT_IDLE,ENABLE);                      /*  使能串口接收空闲中断          */
    
    USART_Cmd(GPS_COM, ENABLE);
    
    Debug(GPS_DEBUG, "GPS Com Initialization is Complete!"endl);
}

/*
    gps串口的DMA配置
*/
static void DMA_Config_GPS(void)
{
    DMA_InitTypeDef DMA_InitStruct;	
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);
    
    DMA_Cmd(GPS_COM_RX_DMA1CHANNEL,DISABLE);
    DMA_DeInit(GPS_COM_RX_DMA1CHANNEL);	
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(GPS_COM->DR));
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)&(pGpsReceve->buff);
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStruct.DMA_BufferSize         = GPS_RX_MAX_BYTE;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(GPS_COM_RX_DMA1CHANNEL,&DMA_InitStruct);                  /*  接收DMA配置                   */
    DMA_ITConfig(GPS_COM_RX_DMA1CHANNEL,DMA_IT_TE,DISABLE);
    DMA_ITConfig(GPS_COM_RX_DMA1CHANNEL,DMA_IT_HT,DISABLE);
    DMA_ITConfig(GPS_COM_RX_DMA1CHANNEL,DMA_IT_TC,DISABLE);

//  DMA_Cmd(GPS_COM_RX_DMA1CHANNEL,DISABLE);
//  DMA_DeInit(GPS_COM_RX_DMA1CHANNEL);	
//  DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(GPS_COM->DR));
//  DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
//  DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
//  DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)0;
//  DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
//  DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
//  DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralDST;
//  DMA_InitStruct.DMA_BufferSize         = 0;
//  DMA_InitStruct.DMA_Priority           = DMA_Priority_VeryHigh;
//  DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
//  DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
//  DMA_Init(SCREEN_COM_TX_DMA1CHANNEL,&DMA_InitStruct);               /*  接收DMA配置                   */ 

//  DMA_Cmd(GPS_COM_TX_DMA1CHANNEL,ENABLE);
    DMA_Cmd(GPS_COM_RX_DMA1CHANNEL,ENABLE);
}


/*
    计算逗号位置
*/
static uint8_t GetComma(char *msg, uint8_t num) {
    uint8_t i = 0,j = 0;
    while (msg[i] != '\0') {
        if(msg[i] == ',') {
            j++;
        }
        
        if(j == num) {
            return i+1;
        }
        
        i++;
    }
    return 0;
}

/*
    GPS数据处理函数
*/
void GpsProcess (void) {
    char      *token;
    uint8_t   tmpPos;
    uint8_t   tmpArg;
    struct tm time;
    time_t    t_time = 0;
    
    if (pGpsProcess->size > 0) {
        token = strtok(pGpsProcess->buff, "$");
  
        while(token != (char*)0) {
            if (strncmp(token, "GNRMC", 5) == 0) {                     /*  RMC报文                       */
                tmpPos = GetComma(token, 1);                           /*  UTC时间                       */    
                if (token[tmpPos] != ',') {
                    time.tm_hour = (token[tmpPos] - '0') * 10 + (token[tmpPos + 1]-'0');
                    time.tm_min  = (token[tmpPos + 2] +  - '0') * 10 + (token[tmpPos + 3]-'0');
                    time.tm_sec  = (token[tmpPos + 4] - '0') * 10 + (token[tmpPos + 5]-'0');
                } else {
                    time.tm_hour = 0;
                    time.tm_min  = 0;
                    time.tm_sec  = 0;
                }
                
                tmpPos = GetComma(token, 9);                           /*  UTC日期                       */
                if (token[tmpPos] != ',') {
                    time.tm_mday = (token[tmpPos] - '0') * 10 + (token[tmpPos + 1]-'0');
                    time.tm_mon  = (token[tmpPos + 2] +  - '0') * 10
                                + (token[tmpPos + 3]-'0') - 1;
                    time.tm_year = (token[tmpPos + 4] - '0') * 10
                                + (token[tmpPos + 5]-'0') + 100;      /*  + 2000 - 1900                  */
                } else {
                    time.tm_mday = 0;
                    time.tm_mon  = 0;
                    time.tm_year = 0;
                }
   
                tmpPos = GetComma(token, 2);
                if (token[tmpPos] == 'A') {                            /*  pps 数据有效                  */
                    t_time = mktime(&time) + 28800;                    /*  东 8 区  + 8 * 60 * 60        */
                                                                       /*  更新一次本地 rtc 时间         */
                    if ((RTC_InitFlag == false) && (screenInfo.ID != UNKNOW)) { 
                        screenMsg.wRtc(&t_time);
                        RTC_InitFlag = true;
                    }
                    
                    if (FPGA_State == FPGA_RESET) {
                        if (GPIOA->IDR & GPIO_Pin_6) {                 /*  PPS LOCK VAILD                */
                            FPGA_State = FPGA_TIMEINIT;
                        } 
                    }
                    
                    if (FPGA_State == FPGA_TIMEINIT) {
                        SpiPackaged(Mask_GPS, t_time);                 /*  配置 FPGA 时间                */
                        screenMsg.rRtc(0);
                        tmpArg = 1;
                        screenMsg.wLock(&tmpArg);
                        screenMsg.wCtrl(&tmpArg);
                        FPGA_State = FPGA_WORK;
                        Debug(GPS_DEBUG, Red(CONFIG)" FPGA GPS Time To %#lx"endl, t_time);
                        Debug(GPS_DEBUG, "Now: %s"endl, asctime(localtime(&t_time)));
                    } 
                }
            }
            
            if (strncmp(token, "GNGGA", 5) == 0) {                     /*  GGA报文                       */
                
            }
            
            token = strtok(NULL, "$");                                 /*  下一帧数据                    */
        }
        
        pGpsProcess->size = 0;
    }
}

/*
    GPS串口服务函数
*/
void GPS_COM_IRQHandler(void)
{
    uint16_t tmpClearF;

    if(USART_GetITStatus(GPS_COM,USART_IT_IDLE))
    {
        tmpClearF  = GPS_COM->SR;
        tmpClearF += GPS_COM->DR;                                      /*  清除中断标志                  */
        
        USART_DMACmd(GPS_COM,USART_DMAReq_Rx,DISABLE);
        DMA_Cmd(GPS_COM_RX_DMA1CHANNEL,DISABLE);                       /*  关闭DMA接收                   */
    
        pGpsReceve->size                   = GPS_RX_MAX_BYTE
                                           - DMA_GetCurrDataCounter(GPS_COM_RX_DMA1CHANNEL);
        pGpsReceve->buff[pGpsReceve->size] = '\0';
        pGpsProcess                        = pGpsReceve;
        
        if (pGpsReceve == &GpsReceveA) {                               /*  切换接收Buff                  */
            pGpsReceve  = &GpsReceveB;
        } else {
            pGpsReceve  = &GpsReceveA;
        }
        
        GPS_COM_RX_DMA1CHANNEL->CMAR = (uint32_t)&(pGpsReceve->buff);
        DMA_SetCurrDataCounter(GPS_COM_RX_DMA1CHANNEL,GPS_RX_MAX_BYTE);
        DMA_Cmd(GPS_COM_RX_DMA1CHANNEL,ENABLE); 
        USART_DMACmd(GPS_COM,USART_DMAReq_Rx,ENABLE);                  /*  使能DMA接收                   */
    }
}

