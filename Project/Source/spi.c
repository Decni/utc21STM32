#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "platform_config.h"
#include "spi.h"
#include "list.h"
#include "flash.h"
#include "screen.h"
#include "shell.h"

tList     *TriList;
tMemory   *TriMem;
tConfig    Config;
struct tm  TimedTrig = {0};

const char *RecordHead    = " Number | Channel |           Timestamp           "endl;
//                          "   01       EO1     2021-09-27 23:18:26.539.906.83\r"
const char *DelayHead     = " Number | Channel | Delay Time(us)"endl;
//                          "   01       EI1     1000000.00     \r"
const char *TimestampHelp = "        "Blue(ts)": timestamp"endl
                            "        [ ] Show All."endl
                            "       [-x] Show The Last x Records.(x = 1,2,3...)"endl
                            "       [-f] Delet Timestamp Records."endl
                            "       [-h] Show ts help."endl;
const char *OutDelayHelp  = "        "Blue(od)": out delay  delta(t) = 0.01 us."endl
                            "        [ ] Show All."endl
                            "   [-pox n] Set POx Value To n us."endl
                            "   [-eox n] Set EOx Value To n us.(n(us). x = 1,2,3...)"endl
                            "       [-h] Show od help."endl;
const char *ResetFpgaHelp = "        "Blue(rf)": reset fpga."endl
                            "        [ ] Reset FPGA."endl
                            "       [-h] Show rf help."endl;

tList   SpiTxList, SpiRxList;                                          /*  spi收发数据链表               */
tMemory SpiTxMem, SpiRxMem;                                            /*  spi收发内存块                 */
uint8_t SpiTxBuff[sizeof(tSpiTxNode) * SPI_TX_MAX_ITEM];
uint8_t SpiRxBuff[sizeof(tSpiRxNode) * SPI_RX_MAX_ITEM];               /*  spi收发缓存区                 */
                                                                       
tSpiRxNode *pSpiRxNode;       
tSpiTxNode *pSpiTxNode;                                                /*  spi收发节点指针               */
tSpiState   SpiState;                                                  /*  spi当前的状态                 */

uint16_t   SpiTimer_rFPGA   = 0;                                       /*  上电500ms复位FPGA             */
uint16_t   SpiTimer_sRecord = 0;                                       /*  触发后 3s 保存数据到 flash    */
uint16_t   SpiTimer_fScreen = 0;                                       /*  触发后 210ms 刷新屏幕         */
uint32_t   SpiTimer_tCntR   = 0;                                       /*  记录同步触发的次数            */
uint32_t   SpiTimer_tCntT   = 0;
tFpgaState FPGA_State;

static void DMA_Config_SPI (void);
static void SpecialPinInit(void);
static void ShellCallback_GetRecord(char* arg);
static void ShellCallback_OutDelay(char *arg);
static void ShellCallback_ResetFPGA (char *arg);

/*  
    SPI初始化函数
*/
 void ComInit_SPI(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;
    
    listInit(&SpiTxList);                                              /*  初始化发送和接收链表          */
    listInit(&SpiRxList);
    memInit(&SpiTxMem,SpiTxBuff, sizeof(tSpiTxNode), SPI_TX_MAX_ITEM, "SpiTxMem");
    memInit(&SpiRxMem,SpiRxBuff, sizeof(tSpiRxNode), SPI_RX_MAX_ITEM, "SpiRxMem");
                                                                       /*  初始化发送和接受内存块        */
    TriList    = &SpiRxList;
    TriMem     = &SpiRxMem;
    FPGA_State = FPGA_UNKNOW;
    SpiState   = SPI_IDLE;
  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);                             /*  NSS                           */
    GPIO_SetBits(GPIOB,GPIO_Pin_12);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;         /*  CLK MOSI                      */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_14;                       /*  MISO                          */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);                             /*  初始化CLK MOSI MISO NSS管脚   */

    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b; 
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_High; 
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_LSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStructure);                                /*  SPI基本配置                   */
    SPI_SSOutputCmd(SPI2, DISABLE);                                    /*  禁止NSS输出                   */
    SPI2->CR1 &= ~(uint16_t)(1 << 10);                                 /*  清除RxOnly位                  */
    
    DMA_Config_SPI();                                                  /*  SPI收发DMA配置                */
    SpecialPinInit();                                                  /*  专用管脚初始化                */
    ShellCmdAdd("ts", ShellCallback_GetRecord, TimestampHelp);
    ShellCmdAdd("od", ShellCallback_OutDelay, OutDelayHelp);
    ShellCmdAdd("rf", ShellCallback_ResetFPGA, ResetFpgaHelp);
    Debug(SPI_DEBUG, "SPI2 Initialization is Complete!"endl);
}

/*
    DMA配置函数
*/
static void DMA_Config_SPI (void) {
    DMA_InitTypeDef DMA_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);
    
    DMA_Cmd(DMA1_Channel4,DISABLE);                                    /*  接收DMA配置                   */
    DMA_DeInit(DMA1_Channel4);	
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI2->DR));
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)0;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStruct.DMA_BufferSize         = 0;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4,&DMA_InitStruct);
    DMA_ClearFlag(DMA1_FLAG_GL4);
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, DISABLE);
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TE, DISABLE);
    DMA_ITConfig(DMA1_Channel4, DMA_IT_HT, DISABLE);
    
    NVIC_InitStruct.NVIC_IRQChannel                   = DMA1_Channel4_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    DMA_Cmd(DMA1_Channel5,DISABLE);                                    /*  发送DMA配置                   */
    DMA_DeInit(DMA1_Channel5);	
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI2->DR));
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)0;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStruct.DMA_BufferSize         = 0;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_High;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5,&DMA_InitStruct);
    DMA_ClearFlag(DMA1_FLAG_GL5);
    DMA_ITConfig(DMA1_Channel5,DMA_IT_TE,DISABLE);
    DMA_ITConfig(DMA1_Channel5,DMA_IT_HT,DISABLE);
    DMA_ITConfig(DMA1_Channel5,DMA_IT_TC,DISABLE);
        
    NVIC_InitStruct.NVIC_IRQChannel                   = DMA1_Channel5_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

/*
    配置SPI为输出模式
*/
static void ConfigSPI_Tx(void) {
    NotifyFPGA(Mode_Transmit);

    DMA_ClearFlag(DMA1_FLAG_GL5);
    DMA_SetCurrDataCounter(DMA1_Channel5, SPI_TX_MAX_BYTE);
    DMA1_Channel5->CCR |= DMA_MemoryInc_Enable;
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_ITConfig(DMA1_Channel5,DMA_IT_TC,ENABLE);
    SPI_Cmd(SPI2, ENABLE);
    DMA_Cmd(DMA1_Channel5,ENABLE);
}

/*
    配置SPI为输入模式
*/
static void ConfigSPI_Rx(void) {
    static uint8_t tVar = 0;
    NotifyFPGA(Mode_Receve);

    DMA_ClearFlag(DMA1_FLAG_GL4);
    DMA_SetCurrDataCounter(DMA1_Channel4, SPI_RX_MAX_BYTE);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
    DMA_ITConfig(DMA1_Channel4,DMA_IT_TC,ENABLE);    
    DMA_Cmd(DMA1_Channel4,ENABLE);
    
    DMA_ClearFlag(DMA1_FLAG_GL5);
    DMA_SetCurrDataCounter(DMA1_Channel5, SPI_RX_MAX_BYTE);
    DMA1_Channel5->CMAR = (uint32_t)&tVar;
    DMA1_Channel5->CCR &= ~DMA_MemoryInc_Enable;
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI2, ENABLE);
    DMA_Cmd(DMA1_Channel5,ENABLE);
}

/*
    SPI接收函数
*/
void SpiReceve (void) {
    uint16_t tSPIDR = 0;
    
    if ((FPGA_State >= FPGA_WORK)
        && (SpiState == SPI_IDLE) 
        && GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_3)
        && screenInfo.TriMode == Mode_SyncTri) {
            
        tNode *pNode;
        if (listGetCount(&SpiRxList) >= SPI_RX_MAX_ITEM) {
            pNode = listRemoveLast(&SpiRxList);
            if (pNode != (tNode*)0) {
                pSpiRxNode = getNodeParent(tSpiRxNode, node, pNode);
            }
        } else {
            
            pSpiRxNode = (tSpiRxNode*)memGet(&SpiRxMem);               /*  申请一个节点，保存接收到的数据*/
        }
        
        while (SPI2->SR & 0x41) {
            tSPIDR += SPI2->DR;                                        /*  清空RXNE OVR 标志位           */
        }

        DMA1_Channel4->CMAR = (uint32_t)&(pSpiRxNode->buff);
        SpiState = SPI_RECEVE;
        ConfigSPI_Rx();                                                /*  配置并启动SPI接收             */
    } else if (SpiState == SPI_RECEVE_DONE) {
        NotifyFPGA(Mode_Transmit);
        listAddFirst(&SpiRxList, &(pSpiRxNode->node));
        Debug(SPI_DEBUG, "Receve Ok. %#llx"endl, *((uint64_t*)&(pSpiRxNode->buff)));
        pSpiRxNode = (tSpiRxNode*)0;
        
        if (SpiTimer_tCntR < SPI_RX_MAX_ITEM) {
            SpiTimer_tCntR++;
        }
        
        if (SpiTimer_tCntT < SPI_RX_MAX_ITEM) {
            SpiTimer_tCntT++;
        }
        
        SpiTimer_fScreen = 0;
        FPGA_State       = FPGA_TRIG;
        SpiState         = SPI_IDLE;
    }
}

/*
    SPI发送函数
*/
void SpiTransmit (void) {
    tNode *tmpNode;

    if ((FPGA_State >= FPGA_WORK) && (SpiState == SPI_IDLE) && (listGetCount(&SpiTxList) > 0)) {
        tmpNode = listRemoveFirst(&SpiTxList);
        if (tmpNode == (tNode*)0) {
            return;
        }
        pSpiTxNode = getNodeParent(tSpiTxNode, node, tmpNode);
        DMA1_Channel5->CMAR = (uint32_t)&(pSpiTxNode->buff);           /*  更新spi DMA发送               */
        SpiState = SPI_TRANSMIT;
        ConfigSPI_Tx();                                                /*  配置并启动SPI发送             */
    } else if (SpiState == SPI_WAIT_FREE) {
        
        if (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == RESET) {  /*  busy标志已经复位              */
            SPI_Cmd(SPI2, DISABLE);
            SpiState = SPI_IDLE;
        }            
    }
}

/*
    打包要发送的数据
*/
void SpiPackaged (tMsgMask mask, uint32_t value) {
    tSpiTxNode *tmpTxNode;
    
    tmpTxNode = (tSpiTxNode*)memGet(&SpiTxMem);                        /*  申请一块内存                  */
    if (tmpTxNode == (tSpiTxNode*)0) {
        Debug(SPI_DEBUG, Red(ERROR)": %s Out of Memory!"endl, SpiTxMem.name);
        return;
    }
    
    tmpTxNode->buff[0] = (uint8_t)( value        & 0x000000FF);
    tmpTxNode->buff[1] = (uint8_t)((value >> 8)  & 0x000000FF);
    tmpTxNode->buff[2] = (uint8_t)((value >> 16) & 0x000000FF);
    tmpTxNode->buff[3] = (uint8_t)((value >> 24) & 0x000000FF);
    tmpTxNode->buff[4] = mask;
    
    listAddLast(&SpiTxList, &(tmpTxNode->node));
    tmpTxNode = (tSpiTxNode*)0;
}

/*
    dma接收完成中断
*/
void DMA1_Channel4_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_IT_TC4))
    {
        DMA_ClearITPendingBit(DMA1_IT_GL4);                            /*  清除全部中断标志              */
        
        SPI_Cmd(SPI2, DISABLE);
        if( SpiState == SPI_RECEVE) {
            DMA_Cmd(DMA1_Channel5,DISABLE);
            DMA_Cmd(DMA1_Channel4,DISABLE);
            SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
            SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
            DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, DISABLE);
            SpiState = SPI_RECEVE_DONE;
        }
    }
}

/*
    dma发送完成中断
*/
void DMA1_Channel5_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_IT_TC5))
    {
        DMA_ClearITPendingBit(DMA1_IT_GL5);                            /*  清除全部中断标志              */
        
        if (SpiState == SPI_TRANSMIT) {
            DMA_Cmd(DMA1_Channel5,DISABLE);
            SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
            DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, DISABLE);
            memFree(&SpiTxMem, (void*)pSpiTxNode);                     /*  释放当前发送节点              */
            pSpiTxNode = (tSpiTxNode*)0;
            SpiState = SPI_WAIT_FREE;
        }
    }
}

/*
    专用管脚初始化
*/
static void SpecialPinInit(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;    
    EXTI_InitTypeDef EXTI_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);                             /*  PA0     复位FPGA              */
    GPIO_SetBits(GPIOA,GPIO_Pin_0);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);                             /*  PA5     定时结束              */
    
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);
    
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5; 
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt; 
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;             /*  定时结束，上升沿锁定          */
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);                             /*  PA6     pps LOCK(1)           */
    
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource6);
    
    EXTI_InitStructure.EXTI_Line    = EXTI_Line6; 
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt; 
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;     /*  上升沿锁定，下降沿守时        */
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    NVIC_InitStructure.NVIC_IRQChannel                   = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);                             /*  PA7     发送1  接收0          */
    GPIO_SetBits(GPIOA,GPIO_Pin_7);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);                             /*  PC0    启动定时触发           */
    GPIO_ResetBits(GPIOC,GPIO_Pin_0);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);                             /*  PC1    刹车                   */
    GPIO_ResetBits(GPIOC,GPIO_Pin_1);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);                             /*  PC2    触发模式               */
    GPIO_ResetBits(GPIOC,GPIO_Pin_2);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);                             /*  PC3    触发通知               */
}

/* 
    外部触发中断9_5
*/
void EXTI9_5_IRQHandler(void) {
    uint8_t tmpArg;
    if (EXTI_GetITStatus(EXTI_Line5)) {                                /*  定时结束 上升沿               */
        EXTI_ClearITPendingBit(EXTI_Line5);
        NotifyFPGA(Mode_Timed_Stop);
        tmpArg = 0x00;
        screenMsg.wTimed(&tmpArg);                                     /*  弹起定时器按键                */
        tmpArg = 0x01;
        screenMsg.wMode(&tmpArg);                                      /*  使能模式切换按键              */
        screenMsg.wBack(&tmpArg);                                      /*  使能返回按键                  */
        screenMsg.wBrake(&tmpArg);                                     /*  使能刹车按键                  */
        Debug(SPI_DEBUG, "Timed Trigger Complete!"endl);
     }
    
    if (EXTI_GetITStatus(EXTI_Line6)) {
        EXTI_ClearITPendingBit(EXTI_Line6);
        if (GPIOA->IDR & GPIO_Pin_6) {                                 /*  pps lock 双沿                 */
            tmpArg = 0x01;
            Debug(SPI_DEBUG, "Clock Lock!"endl);
        } else {
            tmpArg = 0x02; 
            Debug(SPI_DEBUG, "Clock Unlock!"endl);
        }
        if (screenInfo.ID != UNKNOW) {
            screenMsg.wLock(&tmpArg);
        }
    }
}

/*
    通知fpga
*/
void NotifyFPGA(tMode mode) {
    switch (mode) {
        case Mode_Timed_Go:                                            /*  启动定时触发                  */
            GPIO_SetBits(GPIOC,GPIO_Pin_0);
            Debug(SPI_DEBUG, "Start Timed Trigger!"endl);
            break;
        
        case Mode_Timed_Stop:                                          /*  停止定时触发                  */
            GPIO_ResetBits(GPIOC,GPIO_Pin_0);
            Debug(SPI_DEBUG, "Stop Timed Trigger!"endl);
            break;
        
        case Mode_Stop:                                                /*  刹车                          */
            GPIO_ResetBits(GPIOC,GPIO_Pin_1);
            screenInfo.RunMode = Mode_Stop;
            Debug(SPI_DEBUG, "To Brake Mode!"endl);
            break;
        
        case Mode_Run:                                                 /*  取消刹车                      */
            GPIO_SetBits(GPIOC,GPIO_Pin_1);
            screenInfo.RunMode = Mode_Run;
            Debug(SPI_DEBUG, "Out Brake Mode!"endl);
            break;
        
        case Mode_DelayTri:                                            /*  定时触发模式                  */
            GPIO_ResetBits(GPIOC,GPIO_Pin_2);
            screenInfo.TriMode = Mode_DelayTri;
            Debug(SPI_DEBUG, "Convert to Time Mode!"endl);
            break;
        
        case Mode_SyncTri:                                             /*  同步触发模式                  */
            GPIO_SetBits(GPIOC,GPIO_Pin_2);
            screenInfo.TriMode = Mode_SyncTri;
            Debug(SPI_DEBUG, "Convert to Sync Mode!"endl);
            break;
        
        case Mode_Reset:
            GPIO_ResetBits(GPIOA,GPIO_Pin_0);                          /*  复位FPGA                      */
            GPIO_SetBits(GPIOA,GPIO_Pin_0);
            Debug(SPI_DEBUG, "FPGA Reset is Complete!"endl);
            break;
        
        case Mode_Transmit:                                            /*  发送数据                      */
            GPIO_SetBits(GPIOA,GPIO_Pin_7);
            break;
        
        case Mode_Receve:                                              /*  读取数据                      */
            GPIO_ResetBits(GPIOA,GPIO_Pin_7);                  
            break;
        
        default:
            break;
    }
}

/*
    定时器回调函数
*/
void TimerProcess_SPI(void) {
    if (FPGA_State == FPGA_UNKNOW) {
        if (SpiTimer_rFPGA++ >= RST_FPGA_WAITING) {
            NotifyFPGA(Mode_Reset);
            SpiPackaged(Mask_PO1, Config.po1 * CONFIG_FACTOR);
            SpiPackaged(Mask_PO2, Config.po2 * CONFIG_FACTOR);
            SpiPackaged(Mask_PO3, Config.po3 * CONFIG_FACTOR);
            SpiPackaged(Mask_PO4, Config.po4 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO1, Config.eo1 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO2, Config.eo2 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO3, Config.eo3 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO4, Config.eo4 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO5, Config.eo5 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO6, Config.eo6 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO7, Config.eo7 * CONFIG_FACTOR);
            SpiPackaged(Mask_EO8, Config.eo8 * CONFIG_FACTOR);
            FPGA_State = FPGA_RESET;
        }
    }
    
    if (FPGA_State == FPGA_TRIG) {
        if (SpiTimer_fScreen++ >= FLUSH_SCREEN_TIME) {
            screenMsg.wTriRecord(&SpiTimer_tCntT);
            screenMsg.wTriBatch(0);
            SpiTimer_tCntT   = 0;
            SpiTimer_sRecord = 0;
            FPGA_State       = FPGA_TRIG_DONE;
        }
    }
    
    if (FPGA_State == FPGA_TRIG_DONE) {
        if (SpiTimer_sRecord++ >= SAVE_RECORD_TIME) {
            FlashOperate(FlashOp_TimestampSave);                       /*  保存时间戳                    */
            FPGA_State = FPGA_WORK;
        }
    }
}

/*
    获取时间戳记录
*/
static void ShellCallback_GetRecord(char* arg) {
    tTriDataNode *tmpTriNode;
    tNode        *tmpNode;
    int32_t       tmpListCnt;
    struct tm    *tmpDate;
    uint32_t      tmpTime, tmpMs, tmpUs, tmpNs;
    char          tmpBuff[128] = {0};
    uint8_t       tmpIndex = 0;
    
    tmpListCnt = listGetCount(TriList);
    if (tmpListCnt <= 0) {
        return;
    }
    
    if (arg != NULL) {
        if((arg[0] != '-') || (( arg[1] != 'f') && !isdigit(arg[1]))) {
            ShellPakaged(ErrArgument);
            return;
        }

        if( arg[1] == 'f') {
            if (listGetCount(screenInfo.TriList) > 0) {
                memFreeList(TriMem, screenInfo.TriList);
                FlashOperate(FlashOp_TimestampEraser);
                screenMsg.cRecord(0);
                screenMsg.cTriRecord(0);
            }
            return;
        }
        
        tmpListCnt = tmpListCnt <= atoi(&arg[1]) ? tmpListCnt : atoi(&arg[1]);
    }
    
    ShellPakaged("%s", RecordHead);
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    for (uint16_t i = 0; i < tmpListCnt; i++) {
        tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
        tmpDate    = localtime((time_t*)&(tmpTriNode->date));
        tmpTime    = tmpTriNode->time.item.tim / 2;                    /*  以10ns为单位                  */
        tmpMs      = tmpTime / 100000;
        tmpUs      = (tmpTime % 100000) / 100;
        tmpNs      = tmpTime % 100;
                                                                       /*  序号                          */
        tmpIndex   = sprintf(tmpBuff, "   %2u       \033[34;1m", i + 1);
        
        if (tmpTriNode->time.item.num > 3) {                           /*  通道号                        */
            tmpBuff[tmpIndex++]   = 'P';
            tmpBuff[tmpIndex + 1] = '1' + tmpTriNode->time.item.num % 4;
        } else if (tmpTriNode->time.item.num > 0) {
        
            tmpBuff[tmpIndex++]   = 'E';
            tmpBuff[tmpIndex + 1] = '0' + tmpTriNode->time.item.num;
        } else {
        
            tmpBuff[tmpIndex++]   = 'M';
            tmpBuff[tmpIndex + 1] = '0';
        }
        tmpBuff[tmpIndex++]   = 'I';
        tmpIndex++;
        
        tmpIndex += sprintf((char*)&(tmpBuff[tmpIndex]), 
                                    "\033[0m     %.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u.%.3u.%.2u"endl,
                                    tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday,
                                    tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                    tmpMs, tmpUs, tmpNs);              /*  时间戳                        */
        tmpBuff[tmpIndex] = '\0';

        ShellPakaged("%s", tmpBuff);
        
        tmpNode = listGetNext(TriList, tmpNode);
        if (tmpNode == (tNode*)0) {
            break;
        }
    }
    tmpTriNode = (tTriDataNode*)0;  
}

/*  
    获取或设置通道延迟时间
*/
static void ShellCallback_OutDelay(char *arg) {
    uint32_t *tmpStart;
    uint32_t  tmpValue;
    uint8_t   tmpCh;
    char      tmpBuff[128] = {0};
    uint8_t   tmpIndex = 0;
    char     *token, tmpStr[20];
    
    long double tmpInput;
    uint8_t     tmpPre = 0;
    
    tmpStart = (uint32_t*)&Config;
    if (arg == NULL) {                                                 /*  显示所有通道延迟值            */
        ShellPakaged("%s", DelayHead);
        
        for (uint8_t i =0; i < 12; i++) {
            tmpIndex   = sprintf(tmpBuff, "   %2u       \033[34;1m", i + 1);
            
            if (i < 4) {                                               /*  光输出通道                    */
                tmpBuff[tmpIndex++]   = 'P';
                tmpBuff[tmpIndex + 1] = i + 1 + '0';
            } else {                                                   /*  电输出通道                    */
                tmpBuff[tmpIndex++] = 'E';
                tmpBuff[tmpIndex + 1] = i - 3 + '0';
            }
            tmpBuff[tmpIndex++] = 'O';
            tmpIndex++;
            
            tmpIndex += sprintf(&tmpBuff[tmpIndex], "\033[0m     %7u.%.2u"endl,
                                                *tmpStart / 100, *tmpStart % 100);                    
            tmpBuff[tmpIndex] = '\0';
            
            ShellPakaged("%s", tmpBuff);
            tmpStart++;
        }
    } else {                                                           /*  设置指定通道延迟值            */
        
        if((arg[0] != '-') || ((arg[1] != 'p') && (arg[1] != 'e'))) {  /*  参数有效性检测                */
            ShellPakaged(ErrArgument);
            return;
        }
        
        tmpCh = atoi(&arg[3]);
        
        if (tmpCh == 0) {                                              /*  参数有效性检测                */
            ShellPakaged(ErrArgument);
            return;
        }
        
        if (((arg[1] == 'p') && (tmpCh > 4)) || ((arg[1] == 'e') && (tmpCh > 8))) {
            ShellPakaged(ErrArgument);
            return;
        }
        token = strtok(NULL, DELIM);
        if (token == NULL) {                                           /*  参数有效性检测                */
            ShellPakaged(FewArgument);
            return;
        }
        
        sscanf(token, "%[0123456789.]", tmpStr);                       /*  参数过滤                      */
        token = tmpStr;
        
        while (*token != '\0') { 
            
            if (tmpPre > 0) {
                tmpPre++;
            }
            
            if (*token == '.') {
                tmpPre++;
            }

            if (tmpPre > 3){
                *token = '\0';
                ShellPakaged("Exceeds maximum accuracy. delta(t) = 0.01 us"endl);
                break;
            }
            token++;
        }                                                              /*  保留两位小数                  */
        
        sscanf(tmpStr, "%10Lf", &tmpInput);
        
        tmpValue = (uint32_t)(tmpInput * 100);                         /*  单位 10 ns                    */
        
        if (tmpValue > 0x5F5E100) {                                    /*  1秒                           */
            tmpValue = 0x5F5E100;  
            ShellPakaged("The delay time is greater than 1 second."endl);
        }
        
        if (arg[1] == 'p') {
            tmpStart += tmpCh - 1;
            *tmpStart = tmpValue;
            SpiPackaged((tMsgMask)(tmpCh - 1), tmpValue * CONFIG_FACTOR);
        } else {
            
            tmpStart += tmpCh + 3;
            *tmpStart = tmpValue;
            SpiPackaged((tMsgMask)(tmpCh + 3), tmpValue * CONFIG_FACTOR);
        }
            FlashOperate(FlashOp_ConfigSave);
            screenMsg.wSetBatch(0);
    }
}

/*
    复位 FPGA
*/
static void ShellCallback_ResetFPGA (char *arg) {
    NotifyFPGA(Mode_Reset);
    SpiPackaged(Mask_PO1, Config.po1 * CONFIG_FACTOR);
    SpiPackaged(Mask_PO2, Config.po2 * CONFIG_FACTOR);
    SpiPackaged(Mask_PO3, Config.po3 * CONFIG_FACTOR);
    SpiPackaged(Mask_PO4, Config.po4 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO1, Config.eo1 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO2, Config.eo2 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO3, Config.eo3 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO4, Config.eo4 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO5, Config.eo5 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO6, Config.eo6 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO7, Config.eo7 * CONFIG_FACTOR);
    SpiPackaged(Mask_EO8, Config.eo8 * CONFIG_FACTOR);
    FPGA_State = FPGA_RESET;
    
}
