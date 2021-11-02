#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "platform_config.h"
#include "screen.h"
#include "endian.h"
#include "memory.h"
#include "flash.h"
#include "shell.h"

tList   ScreenTxList, ScreenRxList;                                    /*  屏幕串口收发数据链表          */
tMemory ScreenTxMem, ScreenRxMem;                                      /*  屏幕串口收发内存块            */
uint8_t ScreenTxBuff[sizeof(tScreenTxNode) * SCREEN_TX_MAX_ITEM];
uint8_t ScreenRxBuff[sizeof(tScreenRxNode) * SCREEN_RX_MAX_ITEM];      /*  屏幕串口收发缓存区            */
                                                                      
tScreenRxNode *pScreenRxNodeA, *pScreenRxNodeB, *pScreenRxNodeCurr;    /*  屏幕串口接收节点指针          */
tScreenTxNode *pScreenTxNode;                                          /*  屏幕串口发送节点指针          */

tScreenComState ScreenComStateTx, ScreenComStateRx;                    /*  屏幕串口收发端口状态          */

tScreen    screenInfo;                                                 /*  记录屏幕当前的状态            */
tScreenMsg screenMsg;                                                  /*  打包发送给屏幕的消息          */

volatile uint16_t ScreenTimer_TW = 0;                                  /*  Transmit Waiting              */
volatile uint16_t ScreenTimer_PW = 0;                                  /*  Power Waiting 控制上电顺序    */

static void PowerOn_Screen(void);                                      /*  屏幕上电                      */
static void PowerOff_Screen(void);                                     /*  屏幕掉电                      */
static void DMA_Config_Screen(void);                                   /*  屏幕串口DMA配置               */
static void ScreenStateInit (void);                                    /*  屏幕状态初始化函数            */
static void ShellCallback_ScreenPower (char *arg);

const char *ScreenPowerHelp = "        "Blue(sp)": screen power."endl
                              "       [-n] Turn On Screen Power."endl
                              "       [-f] Turn Off Screen Power."endl
                              "       [-h] Show sp help."endl;

/*
    函数功能：屏幕串口初始化
*/
void ComInit_Screen(uint32_t USART_BaudRate) {
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef  NVIC_InitStruct;
    
    listInit(&ScreenTxList);                                           /*  初始化发送和接收链表          */
    listInit(&ScreenRxList);
    memInit(&ScreenTxMem,ScreenTxBuff, sizeof(tScreenTxNode), SCREEN_TX_MAX_ITEM, "ScreenTxMem");
    memInit(&ScreenRxMem,ScreenRxBuff, sizeof(tScreenRxNode), SCREEN_RX_MAX_ITEM, "ScreenRxMem");
                                                                    /*  初始化发送和接受内存块        */ 
    pScreenRxNodeA    = (tScreenRxNode*)memGet(&ScreenTxMem);          /*  初始化接收节点                */
    pScreenRxNodeB    = (tScreenRxNode*)memGet(&ScreenTxMem);
    pScreenRxNodeCurr = pScreenRxNodeA;
    
    RCC_APB2PeriphClockCmd(SCREEN_COM_TX_CLK | SCREEN_COM_RX_CLK       /*  串口管脚时钟                  */
                        | RCC_APB2Periph_AFIO | SCREEN_POWER_CTRL_CLK,ENABLE);
    RCC_APB1PeriphClockCmd(SCREEN_COM_CLK,ENABLE);                     /*  屏幕串口时钟                  */
    
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = SCREEN_POWER_CTRL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(SCREEN_POWER_CTRL_PORT, &GPIO_InitStructure);            /*  屏幕上电管脚                  */
    GPIO_SetBits(SCREEN_POWER_CTRL_PORT,SCREEN_POWER_CTRL_PIN);        /*  关闭屏幕                      */
    
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;    
    GPIO_InitStructure.GPIO_Pin   = SCREEN_COM_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(SCREEN_COM_TX_PORT, &GPIO_InitStructure);                /*  屏幕串口发送管脚              */
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin  = SCREEN_COM_RX_PIN;
    GPIO_Init(SCREEN_COM_RX_PORT, &GPIO_InitStructure);                /*  屏幕串口接收管脚              */
    
    DMA_Config_Screen();                                               /*  屏幕串口DMA配置               */
    
    USART_InitStruct.USART_BaudRate            = USART_BaudRate;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_Init(SCREEN_COM, &USART_InitStruct);                         /*  屏幕串口配置                  */
    
    NVIC_InitStruct.NVIC_IRQChannel                   = SCREEN_COM_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    
    USART_ClearFlag(SCREEN_COM,USART_FLAG_TXE);	
    USART_DMACmd(SCREEN_COM,USART_DMAReq_Rx,ENABLE);                   /*  使能屏幕串口DMA请求           */
    USART_DMACmd(SCREEN_COM,USART_DMAReq_Tx,DISABLE);
    
    USART_ClearFlag(SCREEN_COM,USART_FLAG_IDLE);                       /*  使能屏幕串口空闲和发送中断    */
    USART_ClearFlag(SCREEN_COM,USART_FLAG_TC);  
    USART_ITConfig(SCREEN_COM,USART_IT_IDLE,ENABLE);
    USART_ITConfig(SCREEN_COM,USART_IT_TC,ENABLE);
    
    ScreenComStateTx = ScreenComState_TransmitIdle;                    /*  初始化收发端口状态            */
    ScreenComStateRx = ScreenComState_ReceiveIdle;
    
    ScreenStateInit();                                                 /*  初始化屏幕状态                */
    
    USART_Cmd(SCREEN_COM, ENABLE);

    ShellCmdAdd("sp", ShellCallback_ScreenPower, ScreenPowerHelp);
    Debug(SCREEN_DEBUG, "Screen Com Initialization is Complete!"endl);
}

/*
    屏幕串口DMA配置
*/
static void DMA_Config_Screen(void) {
    DMA_InitTypeDef DMA_InitStruct;
    
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);                  /*  使能DMA1时钟                  */
    
    DMA_Cmd(SCREEN_COM_RX_DMA1CHANNEL,DISABLE);                        /*  屏幕串口接收DMA配置           */
    DMA_DeInit(SCREEN_COM_RX_DMA1CHANNEL);
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(SCREEN_COM->DR));
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)(pScreenRxNodeCurr->buff);
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStruct.DMA_BufferSize         = SCREEN_RX_MAX_BYTE;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(SCREEN_COM_RX_DMA1CHANNEL,&DMA_InitStruct);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_TC, DISABLE);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_TE, DISABLE);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_HT, DISABLE);
    DMA_Cmd(SCREEN_COM_RX_DMA1CHANNEL,ENABLE);
    
    DMA_Cmd(SCREEN_COM_TX_DMA1CHANNEL,DISABLE);                        /*  屏幕串口发送DMA配置           */
    DMA_DeInit(SCREEN_COM_TX_DMA1CHANNEL);
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(SCREEN_COM->DR));
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)0;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStruct.DMA_BufferSize         = 0;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_Low;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(SCREEN_COM_TX_DMA1CHANNEL,&DMA_InitStruct);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_TC, DISABLE);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_TE, DISABLE);
    DMA_ITConfig(SCREEN_COM_TX_DMA1CHANNEL, DMA_IT_HT, DISABLE);
}

/*
    复位屏幕
*/
static void ScreenMsg_Reset (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;    

    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x07;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x35;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x5A;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x53;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xA5;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    清除测试界面触发通道
*/
static void ScreenMsg_cTriChannel (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;

    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;    
}

/*
    更新测试界面触发通道
*/
static void ScreenMsg_wTriChannel (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    
    if (listGetCount(TriList) <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;

    if (tmpTriNode->time.item.num > 3) {
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
    } else if (tmpTriNode->time.item.num > 0) {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
    }
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
    tmpTxNode->msgCnt++;
  
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode  = (tScreenTxNode*)0;
    tmpTriNode = (tTriDataNode*)0;    
}

/*
    清除测试界面触发日期
*/
static void ScreenMsg_cTriDate (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新测试界面触发日期
*/
static void ScreenMsg_wTriDate (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    struct tm     *tmpDate;
    
    if (listGetCount(TriList) <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
    tmpDate    = localtime((time_t*)&(tmpTriNode->date));
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),"%.4u-%.2u-%.2u",
                                 tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday);  
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode  = (tScreenTxNode*)0;
    tmpTriNode = (tTriDataNode*)0;    
}

/*
    清除测试界面触发时刻
*/
static void ScreenMsg_cTriTime (void *arg) {
    tScreenTxNode *tmpTxNode;

    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;    
}

/*
    更新测试界面触发时刻
*/
static void ScreenMsg_wTriTime (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    
    if (listGetCount(TriList) <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
    tmpDate    = localtime((time_t*)&(tmpTriNode->date));
    tmpTime    = tmpTriNode->time.item.tim / 2;                        /*  以10ns为单位                  */
    tmpMs      = tmpTime / 100000;
    tmpUs      = (tmpTime % 100000) / 100;
    tmpNs      = tmpTime % 100;
        
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;

    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                    "%.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                    tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                    tmpMs, tmpUs, tmpNs);
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode  = (tScreenTxNode*)0;
    tmpTriNode = (tTriDataNode*)0;
}

/*
    清除测试界面定时触发时间
*/
static void ScreenMsg_cTriDelay (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新测试界面定时触发时间
*/
static void ScreenMsg_wTriDelay (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    if (screenInfo.Config->triDelay != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]), "%u.%.2u",
                                  screenInfo.Config->triDelay / 100, screenInfo.Config->triDelay % 100);
    } else {
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    }
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    清除测试界面触发记录
*/
static void ScreenMsg_cTriRecord (void *arg) {
    tScreenTxNode *tmpTxNode;

    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x53;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x05;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;    
}

/*
    更新测试界面触发记录
*/
static void ScreenMsg_wTriRecord (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    uint32_t        breakCnt;
    int32_t        tmpListCnt;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    tmpListCnt = listGetCount(TriList);
    if (tmpListCnt <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    if (arg != (void*)0) {
        breakCnt = *(uint32_t*)arg; 
        breakCnt = breakCnt > 5 ? 4 : breakCnt - 1;
    } else {
        breakCnt = 4;
    }
    
    for (uint16_t i = 0; i < tmpListCnt; i++) {
        tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
        tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);              /*  申请一块内存                  */
        if (tmpTxNode == (tScreenTxNode*)0) {
            Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
            return;
        }
        tmpTxNode->msgCnt = 0;
        
        tmpDate    = localtime((time_t*)&(tmpTriNode->date));
        tmpTime    = tmpTriNode->time.item.tim / 2;                    /*  以10ns为单位                  */
        tmpMs      = tmpTime / 100000;
        tmpUs      = (tmpTime % 100000) / 100;
        tmpNs      = tmpTime % 100;
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x52;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x05;
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
        
        if (tmpTriNode->time.item.num > 3) {
            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
        } else if (tmpTriNode->time.item.num > 0) {
        
            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
        } else {
        
            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
        }
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
        tmpTxNode->msgCnt++;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
        
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                    "%.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                    tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday,
                                    tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                    tmpMs, tmpUs, tmpNs);               
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';'; 
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        
        listAddFirst(&ScreenTxList, &(tmpTxNode->node));
        
        if (i == breakCnt) {
            break;
        }
        
        tmpNode = listGetNext(TriList, tmpNode);
        if (tmpNode == (tNode*)0) {
            return;
        }
    }
    tmpTriNode = (tTriDataNode*)0;
    tmpTxNode  = (tScreenTxNode*)0;
}

/*
    清除触发记录
*/
static void ScreenMsg_cRecord (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x53;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新触发记录
*/
static void ScreenMsg_wRecord (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    int32_t        tmpListCnt;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    tmpListCnt = listGetCount(TriList);
    if (tmpListCnt <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
            return;
    }

    for (uint16_t i = 0; i < tmpListCnt; i++) {
        tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);
        tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);              /*  申请一块内存                  */
        if (tmpTxNode == (tScreenTxNode*)0) {
            Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
            return;
        }
        tmpTxNode->msgCnt = 0;
        
        tmpDate    = localtime((time_t*)&(tmpTriNode->date));
        tmpTime    = tmpTriNode->time.item.tim / 2;                    /*  以10ns为单位                  */
        tmpMs      = tmpTime / 100000;
        tmpUs      = (tmpTime % 100000) / 100;
        tmpNs      = tmpTime % 100;

        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x52;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;

        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';

        if (tmpTriNode->time.item.num > 3) {
            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
        } else if (tmpTriNode->time.item.num > 0) {

            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
        } else {

            tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
            tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
        }
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
        tmpTxNode->msgCnt++;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';

        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                    "%.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                    tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday,
                                    tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                    tmpMs, tmpUs, tmpNs);               
        tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

        if (arg != (void*)0) {
            listAddFirst(&ScreenTxList, &(tmpTxNode->node));
            if (i == *(uint32_t*)arg - 1) {
                break;
            }
        } else {
            
            listAddFirst(&ScreenTxList, &(tmpTxNode->node));
        }

        tmpNode = listGetNext(TriList, tmpNode);
        if (tmpNode == (tNode*)0) {
            return;
        }
    }
    tmpTxNode  = (tScreenTxNode*)0;
    tmpTriNode = (tTriDataNode*)0;
}

/*
    切换到选项界面
*/
static void ScreenMsg_toOPTION (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;

    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新光输出通道1
*/
static void ScreenMsg_wPO1 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    if (screenInfo.Config->po1 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->po1 / 100, screenInfo.Config->po1 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新光输出通道2
*/
static void ScreenMsg_wPO2 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    if (screenInfo.Config->po2 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->po2 / 100, screenInfo.Config->po2 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新光输出通道3
*/
static void ScreenMsg_wPO3 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    if (screenInfo.Config->po3 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->po3 / 100, screenInfo.Config->po3 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新光输出通道4
*/
static void ScreenMsg_wPO4 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    if (screenInfo.Config->po4 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->po4 / 100, screenInfo.Config->po4 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道1
*/
static void ScreenMsg_wEO1 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x05;
    if (screenInfo.Config->eo1 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo1 / 100, screenInfo.Config->eo1 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道2
*/
static void ScreenMsg_wEO2 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x06;
    if (screenInfo.Config->eo2 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo2 / 100, screenInfo.Config->eo2 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道3
*/
static void ScreenMsg_wEO3 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x07;
    if (screenInfo.Config->eo3 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo3 / 100, screenInfo.Config->eo3 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道4
*/
static void ScreenMsg_wEO4 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x08;
    if (screenInfo.Config->eo4 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo4 / 100, screenInfo.Config->eo4 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道5
*/
static void ScreenMsg_wEO5 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x09;
    if (screenInfo.Config->eo5 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo5 / 100, screenInfo.Config->eo5 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道6
*/
static void ScreenMsg_wEO6 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x0A;
    if (screenInfo.Config->eo6 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo6 / 100, screenInfo.Config->eo6 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道7
*/
static void ScreenMsg_wEO7 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x0B;
    if (screenInfo.Config->eo7 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                          "%u.%.2u", screenInfo.Config->eo7 / 100, screenInfo.Config->eo7 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    更新电输出通道8
*/
static void ScreenMsg_wEO8 (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x10;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x0C;
    if (screenInfo.Config->eo8 != 0) {
        tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                           "%u.%.2u", screenInfo.Config->eo8 / 100, screenInfo.Config->eo8 % 100);
    } else {
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    } 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    批量更新设置数据
*/
static void ScreenMsg_wSetBatch (void *arg) {
    tScreenTxNode *tmpTxNode;
    uint32_t      *tmpStart = (uint32_t*)&Config;
    
    for (uint8_t i = 0; i < 3; i++) {
        tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
        if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
            return;
        }
        tmpTxNode->msgCnt = 0;
        
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x12;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 4 * i + 1;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        
        if (tmpStart[4 * i] != 0) {
            tmpTxNode->buff[tmpTxNode->msgCnt] =
                    sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt + 1]),
                               "%u.%.2u",  tmpStart[4 * i] / 100,  tmpStart[4 * i] % 100);
            tmpTxNode->msgCnt += tmpTxNode->buff[tmpTxNode->msgCnt] + 1;
        } else {
            
            tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
            tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        } 
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 4 * i + 2;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        if (tmpStart[4 * i + 1] != 0) {
            tmpTxNode->buff[tmpTxNode->msgCnt] =
                    sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt + 1]),
                               "%u.%.2u",  tmpStart[4 * i + 1] / 100,  tmpStart[4 * i + 1] % 100);
            tmpTxNode->msgCnt += tmpTxNode->buff[tmpTxNode->msgCnt] + 1;
        } else {
            
            tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
            tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        }
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 4 * i + 3;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        if (tmpStart[4 * i + 2] != 0) {
            tmpTxNode->buff[tmpTxNode->msgCnt] =
                    sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt + 1]),
                               "%u.%.2u",  tmpStart[4 * i + 2] / 100,  tmpStart[4 * i + 2] % 100);
            tmpTxNode->msgCnt += tmpTxNode->buff[tmpTxNode->msgCnt] + 1;
        } else {
            
            tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
            tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        }
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 4 * i + 4;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
        if (tmpStart[4 * i + 3] != 0) {
            tmpTxNode->buff[tmpTxNode->msgCnt] =
                    sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt + 1]),
                               "%u.%.2u",  tmpStart[4 * i + 3] / 100,  tmpStart[4 * i + 3] % 100);
            tmpTxNode->msgCnt += tmpTxNode->buff[tmpTxNode->msgCnt] + 1;
        } else {
            
            tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
            tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
        }
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
        
        listAddLast(&ScreenTxList, &(tmpTxNode->node));
        tmpTxNode = (tScreenTxNode*)0;
    }
}

/*
    设置RTC日期
*/
static void  ScreenMsg_wRtc (void *arg) {
    tScreenTxNode *tmpTxNode;
    struct tm     *tmpTm;
    time_t        *time;
    
    if (arg == (struct tm *)0) {
        return;
    }
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    time = (time_t *)arg;
    tmpTm = localtime(time);
    
#define ToBCD(n)  ((((n) / 10) << 4) + ((n) % 10)) 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x81;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_sec);       /*  sec                           */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_min);       /*  min                           */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_hour);      /*  hour                          */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_mday);      /*  day                           */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_wday);      /*  week                          */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_mon + 1);   /*  mon                           */
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ToBCD(tmpTm->tm_year - 100);/*  year                          */ 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;                       
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    读取RTC日期
*/
static void  ScreenMsg_rRtc (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x82;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}
/*
    批量更新触发值
*/
static void ScreenMsg_wTriBatch (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *pNode;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    pNode = listGetFirst(TriList);
    if (pNode == (tNode*)0) {
        return;
    }
    tmpTriNode = getNodeParent(tTriDataNode, node, pNode);
    
    if (listGetCount(&ScreenTxList) >= SCREEN_TX_MAX_ITEM) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpDate    = localtime((time_t*)&(tmpTriNode->date));
    tmpTime    = tmpTriNode->time.item.tim / 2;                        /*  以10ns为单位                  */
    tmpMs      = tmpTime / 100000;
    tmpUs      = (tmpTime % 100000) / 100;
    tmpNs      = tmpTime % 100;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x12;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    if (tmpTriNode->time.item.num > 3) {
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
    } else if (tmpTriNode->time.item.num > 0) {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
    } else {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
    }
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
    tmpTxNode->msgCnt++;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x02;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x0A;
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                    "%.4u-%.2u-%.2u",
                                    tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday);
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x13;
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                    "%.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                    tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                    tmpMs, tmpUs, tmpNs);
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddFirst(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    追加新的测试数据到测试界面记录表格
*/
static void ScreenMsg_wNrToTest (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    int32_t        tmpListCnt;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    tmpListCnt = listGetCount(TriList);
    if (tmpListCnt <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);           /*  获取触发节点                  */
    
    if (listGetCount(&ScreenTxList) >= SCREEN_TX_MAX_ITEM) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
        
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpDate    = localtime((time_t*)&(tmpTriNode->date));
    tmpTime    = tmpTriNode->time.item.tim / 2;                        /*  以10ns为单位                  */
    tmpMs      = tmpTime / 100000;
    tmpUs      = (tmpTime % 100000) / 100;
    tmpNs      = tmpTime % 100;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x52;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x05;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
    
    if (tmpTriNode->time.item.num > 3) {
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
    } else if (tmpTriNode->time.item.num > 0) {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
    } else {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
    }
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
    tmpTxNode->msgCnt++;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
    
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                "%.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday,
                                tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                tmpMs, tmpUs, tmpNs);               
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';'; 
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    
    tmpTriNode = (tTriDataNode*)0;
    tmpTxNode  = (tScreenTxNode*)0;
}

/*
    追加新的测试数据到记录界面记录表格
*/
static void ScreenMsg_wNrToRecord (void *arg) {
    tScreenTxNode *tmpTxNode;
    tTriDataNode  *tmpTriNode;
    tNode         *tmpNode;
    int32_t        tmpListCnt;
    struct tm     *tmpDate;
    uint32_t       tmpTime, tmpMs, tmpUs, tmpNs;
    
    tmpListCnt = listGetCount(TriList);
    if (tmpListCnt <= 0) {
        return;
    }
    
    tmpNode = listGetFirst(TriList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    
    tmpTriNode = getNodeParent(tTriDataNode, node, tmpNode);           /*  获取触发节点                  */
    
    if (listGetCount(&ScreenTxList) >= SCREEN_TX_MAX_ITEM) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpDate    = localtime((time_t*)&(tmpTriNode->date));
    tmpTime    = tmpTriNode->time.item.tim / 2;                        /*  以10ns为单位                  */
    tmpMs      = tmpTime / 100000;
    tmpUs      = (tmpTime % 100000) / 100;
    tmpNs      = tmpTime % 100;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x52;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x01;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = '0';
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
    
    if (tmpTriNode->time.item.num > 3) {
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'P';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num % 4;
    } else if (tmpTriNode->time.item.num > 0) {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'E';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0' + tmpTriNode->time.item.num;
    } else {
    
        tmpTxNode->buff[tmpTxNode->msgCnt++]   = 'M';
        tmpTxNode->buff[tmpTxNode->msgCnt + 1] = '0';
    }
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 'I';
    tmpTxNode->msgCnt++;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';';
    
    tmpTxNode->msgCnt += sprintf((char*)&(tmpTxNode->buff[tmpTxNode->msgCnt]),
                                "%.4u-%.2u-%.2u %.2u:%.2u:%.2u.%.3u.%.3u.%.2u",
                                tmpDate->tm_year + 1900, tmpDate->tm_mon + 1, tmpDate->tm_mday,
                                tmpDate->tm_hour, tmpDate->tm_min, tmpDate->tm_sec,
                                tmpMs, tmpUs, tmpNs);               
    tmpTxNode->buff[tmpTxNode->msgCnt++] = ';'; 
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    
    tmpTriNode = (tTriDataNode*)0;
    tmpTxNode  = (tScreenTxNode*)0;
}

/*
    显示 LOCK 信号
*/
static void ScreenMsg_wLock (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x23;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x0B;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = (*(uint8_t*)arg) & 0x03; 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    使能或禁止控件
*/
static void ScreenMsg_wCtrl (void *arg) {
    tScreenTxNode *tmpTxNode;
    
    tmpTxNode = (tScreenTxNode*)memGet(&ScreenTxMem);                  /*  申请一块内存                  */
    if (tmpTxNode == (tScreenTxNode*)0) {
        Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
        return;
    }
    tmpTxNode->msgCnt = 0;
    
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xEE;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xB1;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x04;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x03;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x00;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0x07;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = (*(uint8_t*)arg) & 0x01; 
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFC;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    tmpTxNode->buff[tmpTxNode->msgCnt++] = 0xFF;
    
    listAddLast(&ScreenTxList, &(tmpTxNode->node));
    tmpTxNode = (tScreenTxNode*)0;
}

/*
    屏幕串口发送函数
*/
void ScreenTransmit (void) {
    tNode *tmpNode;
    
    if (screenInfo.ID == UNKNOW) {
        return;
    }
    
    if ((ScreenComStateTx == ScreenComState_TransmitIdle) && (listGetCount(&ScreenTxList) > 0)) {
        tmpNode = listRemoveFirst(&ScreenTxList);
        if (tmpNode == (tNode*)0) {
            return;
        }
        pScreenTxNode = getNodeParent(tScreenTxNode, node, tmpNode);
        
        DMA_ClearFlag(DMA1_FLAG_GL7);
        SCREEN_COM_TX_DMA1CHANNEL->CMAR = (uint32_t)&(pScreenTxNode->buff);
        DMA_SetCurrDataCounter(SCREEN_COM_TX_DMA1CHANNEL,pScreenTxNode->msgCnt);
        USART_DMACmd(SCREEN_COM,USART_DMAReq_Tx,ENABLE);
        DMA_Cmd(SCREEN_COM_TX_DMA1CHANNEL,ENABLE);                     /*  更新并使能屏幕串口DMA发送     */
        
        ScreenComStateTx = ScreenComState_Transmitting;
    }
}

/*
    屏幕串口接收函数
*/
void ScreenReceve (void) {
    if (ScreenComStateRx == ScreenComState_ReceiveCorrect) {
        if (pScreenRxNodeCurr == pScreenRxNodeA) {
            listAddLast(&ScreenRxList, &(pScreenRxNodeB->node));       /*  将接收到的数据插入接收链表    */
            pScreenRxNodeB = (tScreenRxNode*)memGet(&ScreenRxMem);     /*  获取新的节点                  */
            if (pScreenRxNodeB != 0) {
                pScreenRxNodeB->msgCnt = 0;
            } else {
                Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
            }
        } else {
            
            listAddLast(&ScreenRxList, &(pScreenRxNodeA->node));       /*  将接收到的数据插入接收链表    */
            pScreenRxNodeA = (tScreenRxNode*)memGet(&ScreenRxMem);     /*  获取新的节点                  */
            if (pScreenRxNodeA != 0) {
                pScreenRxNodeA->msgCnt = 0;
            } else {
                Debug(SCREEN_DEBUG, Red(ERROR)": Out of Memrmory!"endl);
            }
        }
        ScreenComStateRx = ScreenComState_ReceiveIdle;                 /*  更新屏幕串口接收端状态        */
    }  
}

/*
    处理接收到的消息
*/
void ScreenProcess(void) {
    tScreenRxNode *tmpRxNode;
    tNode         *tmpNode;
    uint8_t       *pMsgEnd, *pMsgIndex;
    uint8_t        tmpArg;
    uint32_t       outDelay = 0;
    uint32_t       triDelay = 0;
    long double    tmpData  = 0;
    struct tm      rtcDate  = {0};
    time_t         t_time   = 0;
    
    
    if (listGetCount(&ScreenRxList) <= 0) {                            /*  接收链表有数据                */
        return;
    }
    
    tmpNode = listRemoveFirst(&ScreenRxList);
    if (tmpNode == (tNode*)0) {
        return;
    }
    tmpRxNode = getNodeParent(tScreenRxNode, node, tmpNode);
    
    if (tmpRxNode->msgCnt < 5) {
        return;
    }
    pMsgIndex = tmpRxNode->buff;
    pMsgEnd   = pMsgIndex + tmpRxNode->msgCnt;

    while (pMsgIndex < pMsgEnd) {
        
        if (*pMsgIndex != 0xEE) {                                      /*  检索包头                      */
            pMsgIndex++;
            continue;
        }
        pMsgIndex++;
        if (*pMsgIndex == 0xF7) {                                      /*  读取 RTC 日期                 */
            pMsgIndex++;
            rtcDate.tm_year = ((*pMsgIndex) >> 4) * 10 + ((*pMsgIndex) & 0x0F) + 100;
            pMsgIndex++;                                               /*   year + 2000 - 1900           */
            rtcDate.tm_mon  = ((*pMsgIndex) >> 4) * 10 + ((*pMsgIndex) & 0x0F) - 1;
            pMsgIndex += 2;                                            /*  mon - 1                       */
            rtcDate.tm_mday = ((*pMsgIndex) >> 4) * 10 + ((*pMsgIndex) & 0x0F);
            t_time = mktime(&rtcDate);
            SpiPackaged(Mask_GPSB, t_time);
            Debug(SCREEN_DEBUG, Red(CONFIG)" FPGA IRIG-B Time to %#lx"endl, t_time);
            Debug(SCREEN_DEBUG, "Now: %s"endl, asctime(&rtcDate));
            pMsgIndex += 4;
        } else if (*pMsgIndex == 0x07) {                               /*  屏幕复位                      */
            
            pMsgIndex++;
            if (*((uint32_t*)pMsgIndex) == 0xFFFFFCFF) {               /*  校验包尾                      */
                screenInfo.Power = true;
                screenMsg.wSetBatch(0);
                screenMsg.wTriDelay(0);
                if (FPGA_State >= FPGA_WORK) {
                    tmpArg = 1;
                } else {
                    tmpArg = 0;
                    screenMsg.rRtc(0);
                }
                screenMsg.wLock(&tmpArg);
                screenMsg.wCtrl(&tmpArg);
                screenMsg.wTriRecord(0);
                screenMsg.wRecord(0);                                  /*  更新缓存在 flash 中的数据     */
                Debug(SCREEN_DEBUG, "Screen Power On."endl);
            }
        } else if (screenInfo.Power && (*pMsgIndex == 0xB1)) {         /*  控件消息                      */
            
            pMsgIndex++;
            switch (*pMsgIndex) {
                case 0x01:                                             /*  当前画面ID通知                */
                    screenInfo.ID  = (tScreenId)(*(pMsgIndex + 2));
                    pMsgIndex     += 3;
                    break;
                
                case 0x27:                                             /*  动画播放完毕                  */
                    if (((tScreenId)(*(pMsgIndex + 2)) == SPLASH) && (*(pMsgIndex + 4) == 1)) {
                        screenMsg.toOPTION(0);
                        screenInfo.ID = OPTION;
                        Debug(SCREEN_DEBUG, "Screen startup is complete."endl);
                    }
                    pMsgIndex += 5;
                    break;

                case 0x11:
                    switch (*(pMsgIndex + 2)) {
                        
                        case OPTION:                                   /*  选项界面                      */
                            if (*(pMsgIndex + 4) == 1) {               /*  测试按键按下                  */
      
                            } else {                                   /*  设置按键按下                  */

                            }
                            pMsgIndex += 8;
                            break;
                            
                        case SETTING:                                  /*  设置界面                      */
                            if (*(pMsgIndex + 4) == 0x0D) {            /*  返回按键                      */
                                FlashOperate(FlashOp_ConfigSave);      /*  保存配置参数到flash           */
                                pMsgIndex += 8;
                                break;
                            }

                            switch (*(pMsgIndex + 4)) {
                                case 0x01:                             /*  光输出1                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->po1 = outDelay;
                                    SpiPackaged(Mask_PO1, screenInfo.Config->po1 * CONFIG_FACTOR);
                                    break;
                                case 0x02:                             /*  光输出2                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->po2 = outDelay;
                                    SpiPackaged(Mask_PO2, screenInfo.Config->po2 * CONFIG_FACTOR);
                                    break;
                                case 0x03:                             /*  光输出3                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->po3 = outDelay;
                                    SpiPackaged(Mask_PO3, screenInfo.Config->po3 * CONFIG_FACTOR);
                                    break;
                                case 0x04:                             /*  光输出4                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->po4 = outDelay;
                                    SpiPackaged(Mask_PO4, screenInfo.Config->po4 * CONFIG_FACTOR);
                                    break;
                                case 0x05:                             /*  电输出1                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo1 = outDelay;
                                    SpiPackaged(Mask_EO1, screenInfo.Config->eo1 * CONFIG_FACTOR);
                                    break;
                                case 0x06:                             /*  电输出2                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo2 = outDelay;
                                    SpiPackaged(Mask_EO2, screenInfo.Config->eo2 * CONFIG_FACTOR);
                                    break;
                                case 0x07:                             /*  电输出3                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo3 = outDelay;
                                    SpiPackaged(Mask_EO3, screenInfo.Config->eo3 * CONFIG_FACTOR);
                                    break;
                                case 0x08:                             /*  电输出4                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo4 = outDelay;
                                    SpiPackaged(Mask_EO4, screenInfo.Config->eo4 * CONFIG_FACTOR);
                                    break;
                                case 0x09:                             /*  电输出5                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo5 = outDelay;
                                    SpiPackaged(Mask_EO5, screenInfo.Config->eo5 * CONFIG_FACTOR);
                                    break;
                                case 0x0A:                             /*  电输出6                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo6 = outDelay;
                                    SpiPackaged(Mask_EO6, screenInfo.Config->eo6 * CONFIG_FACTOR);
                                    break;
                                case 0x0B:                             /*  电输出7                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo7 = outDelay;
                                    SpiPackaged(Mask_EO7, screenInfo.Config->eo7 * CONFIG_FACTOR);
                                    break;
                                case 0x0C:                             /*  电输出8                       */
                                    if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        outDelay = (uint32_t)(tmpData * 100);
                                    }

                                    screenInfo.Config->eo8 = outDelay;
                                    SpiPackaged(Mask_EO8, screenInfo.Config->eo8 * CONFIG_FACTOR);
                                    break;
                                default:
                                    break;
                            }
                            pMsgIndex += 7;
                            break;
                            
                        case TESTING:                                  /*  测试界面                      */
                            if (*(pMsgIndex + 4) == 0x04) {            /*  定时触发值                    */
                                if (*(pMsgIndex + 6) != 0) {
                                        sscanf((const char*)(pMsgIndex + 6), 
                                                         "%Lf[0123456789.]", &tmpData);
                                        triDelay = (uint32_t)(tmpData * 100);
                                    }

                                screenInfo.Config->triDelay = triDelay;
                                SpiPackaged(Mask_triDelay, screenInfo.Config->triDelay * CONFIG_FACTOR);
                                FlashOperate(FlashOp_ConfigSave);
                                pMsgIndex += 7;
                                break;
                            }
                            
                            switch (*(pMsgIndex + 4)) {
                                
                                case 0x06:                             /*  启动定时触发                  */
                                    NotifyFPGA(Mode_DelayGo);
                                    break;

                                case 0x07:                             /*  刹车按键                      */
                                    if (*(pMsgIndex + 7)) {            /*  使能刹车                      */
                                        NotifyFPGA(Mode_Stop);
                                    } else {
                                        NotifyFPGA(Mode_Run);
                                    }
                                    break;
                                
                                case 0x08:                             /*  返回按键                      */
                                    FlashOperate(FlashOp_ConfigSave);
                                    NotifyFPGA(Mode_Stop);
                                    break;
                                
                                case 0x09:                             /*  定时、同步模式转换            */
                                    if (*(pMsgIndex + 7)) {            /*  按下 同步模式                 */
                                        NotifyFPGA(Mode_SyncTri);
                                    } else {
                                        NotifyFPGA(Mode_DelayTri);
                                        if (SpiTimer_tCntR > 0) {
                                            screenMsg.wRecord(&SpiTimer_tCntR);
                                            SpiTimer_tCntR = 0;
                                        }
                                    }
                                    break;
                                
                                case 0x0A:                             /*  查看记录                      */
                                    break;
                                
                                default:
                                    break;
                            }
                            pMsgIndex += 8;
                            break;
                        
                        case RECORD:                                   /*  数据记录界面                  */
                            if (*(pMsgIndex + 4) == 0x03) {            /*  删除记录键按下                */
                                memFreeList(TriMem, screenInfo.TriList);
                                FlashOperate(FlashOp_TimestampEraser);
                            } else if (*(pMsgIndex + 4) == 0x02) {
                                                                       /*  返回键按下                    */
                            }
                            pMsgIndex += 8;
                            break;
                    }
                    break;
                
                default:
                    break;
            }
        }

        while (*pMsgIndex == 0xFF || *pMsgIndex == 0xFC) {
            pMsgIndex++;
        }
    }
        
    memFree(&ScreenRxMem, (void*)tmpRxNode);                           /*  释放当前节点                  */
}

/*
    屏幕定时相关，在定时器中调用
*/
void TimeoutProcess_Screen(void) {
    if (ScreenComStateTx == ScreenComState_TransmitWaiting) {          /*  两条指令之间的间隔时长        */
        ScreenTimer_TW++;
        if (ScreenTimer_TW >= TX_WAITING_TIME) {
            ScreenComStateTx = ScreenComState_TransmitIdle;
        }
    }
    
    if (screenInfo.ID == UNKNOW) {                                      /*  屏幕未连接时，1s发送复位信号  */
        ScreenTimer_PW++;
        if (ScreenTimer_PW >= POWER_WAITING_TIME) {
            if (screenInfo.Power == false) {                           /*  未上电                        */
                PowerOn_Screen();
                screenInfo.Power = true;
            } else if (ScreenTimer_PW >= POWER_RESET_TIME) {           /*  已上电                        */
                
                screenMsg.Reset(0);                                    /*  复位屏幕                      */
                ScreenTimer_PW = 0;
            }
        }
    }
}

/*
    屏幕串口中断服务函数
*/
void SCREEN_COM_IRQHandler(void)
{
    uint16_t      tmpClearF = 0; 
    
    if(USART_GetITStatus(SCREEN_COM,USART_IT_IDLE))                    /*  接收完成中断                  */
    {
        
        tmpClearF  = SCREEN_COM->SR;
        tmpClearF += SCREEN_COM->DR;                                   /*  清除中断标志                  */
        
        USART_DMACmd(SCREEN_COM,USART_DMAReq_Rx,DISABLE);              /*  禁止串口接收DMA请求           */
        DMA_Cmd(SCREEN_COM_RX_DMA1CHANNEL,DISABLE);                    /*  关闭DMA接收                   */
        
        pScreenRxNodeCurr->msgCnt = SCREEN_RX_MAX_BYTE
                                  - DMA_GetCurrDataCounter(SCREEN_COM_RX_DMA1CHANNEL); 
                                                                       /*  获取接收到的长度              */

        if (pScreenRxNodeCurr == pScreenRxNodeA) {                     /*  切换接收节点                  */
            if (pScreenRxNodeB != (tScreenRxNode*)0) {
                pScreenRxNodeCurr = pScreenRxNodeB;
            } else {
                Debug(SCREEN_DEBUG, Red(ERROR)": Screen Receve Buffer Invalid!"endl);
            }
        } else {
            
            if (pScreenRxNodeA != (tScreenRxNode*)0) {
                pScreenRxNodeCurr = pScreenRxNodeA;
            } else {
                Debug(SCREEN_DEBUG, Red(ERROR)": Screen Receve Buffer Invalid!"endl);
            }
        }
        
        ScreenComStateRx = ScreenComState_ReceiveCorrect;              /*  切换屏幕串口接收端状态        */
        
        SCREEN_COM_RX_DMA1CHANNEL->CMAR = (uint32_t)(pScreenRxNodeCurr->buff);
        DMA_SetCurrDataCounter(SCREEN_COM_RX_DMA1CHANNEL,SCREEN_RX_MAX_BYTE);
        DMA_Cmd(SCREEN_COM_RX_DMA1CHANNEL,ENABLE); 
        USART_DMACmd(SCREEN_COM,USART_DMAReq_Rx,ENABLE);		       /*  更新并使能DMA控制器           */
    }
    
    if(USART_GetITStatus(SCREEN_COM,USART_IT_TC))                      /*  发送完成中断                  */
    {
        USART_ClearITPendingBit(SCREEN_COM,USART_IT_TC);
        
        USART_DMACmd(SCREEN_COM,USART_DMAReq_Tx,DISABLE);		
        DMA_Cmd(SCREEN_COM_TX_DMA1CHANNEL,DISABLE);                    /*  关闭DMA发送                   */
        
        memFree(&ScreenTxMem, (void*)pScreenTxNode);                   /*  释放当前发送节点              */
        pScreenTxNode = (tScreenTxNode*)0;
        
        ScreenComStateTx = ScreenComState_TransmitWaiting;             /*  切换screen串口状态            */
        ScreenTimer_TW   = 0;
    }
}

/*
    屏幕状态初始化函数
*/
static void ScreenStateInit (void) {
    screenMsg.Reset       = ScreenMsg_Reset;
    screenMsg.cTriChannel = ScreenMsg_cTriChannel;
    screenMsg.wTriChannel = ScreenMsg_wTriChannel;
    screenMsg.cTriDate    = ScreenMsg_cTriDate;
    screenMsg.wTriDate    = ScreenMsg_wTriDate;
    screenMsg.cTriTime    = ScreenMsg_cTriTime;
    screenMsg.wTriTime    = ScreenMsg_wTriTime;
    screenMsg.cTriDelay   = ScreenMsg_cTriDelay;
    screenMsg.wTriDelay   = ScreenMsg_wTriDelay;
    screenMsg.cTriRecord  = ScreenMsg_cTriRecord;
    screenMsg.wTriRecord  = ScreenMsg_wTriRecord;
    screenMsg.cRecord     = ScreenMsg_cRecord;
    screenMsg.wRecord     = ScreenMsg_wRecord;
    screenMsg.toOPTION    = ScreenMsg_toOPTION;
    screenMsg.wPO1        = ScreenMsg_wPO1;
    screenMsg.wPO2        = ScreenMsg_wPO2;
    screenMsg.wPO3        = ScreenMsg_wPO3;
    screenMsg.wPO4        = ScreenMsg_wPO4;
    screenMsg.wEO1        = ScreenMsg_wEO1;
    screenMsg.wEO2        = ScreenMsg_wEO2;
    screenMsg.wEO3        = ScreenMsg_wEO3;
    screenMsg.wEO4        = ScreenMsg_wEO4;
    screenMsg.wEO5        = ScreenMsg_wEO5;
    screenMsg.wEO6        = ScreenMsg_wEO6;
    screenMsg.wEO7        = ScreenMsg_wEO7;
    screenMsg.wEO8        = ScreenMsg_wEO8;
    screenMsg.wSetBatch   = ScreenMsg_wSetBatch;
    screenMsg.rRtc        = ScreenMsg_rRtc;
    screenMsg.wRtc        = ScreenMsg_wRtc;
    screenMsg.wTriBatch   = ScreenMsg_wTriBatch;
    screenMsg.wNrToTest   = ScreenMsg_wNrToTest;
    screenMsg.wNrToRecord = ScreenMsg_wNrToRecord;
    screenMsg.wLock       = ScreenMsg_wLock;
    screenMsg.wCtrl       = ScreenMsg_wCtrl;
    
    screenInfo.ID         = UNKNOW;
    screenInfo.Power      = false;
    screenInfo.TriList    = TriList;
    screenInfo.Config     = &Config;
    screenInfo.RunMode    = Mode_Stop;
    screenInfo.TriMode    = Mode_DelayTri;
}

/*
    函数功能：给屏幕供电
*/
__STATIC_INLINE void PowerOn_Screen(void) {
    GPIO_ResetBits(SCREEN_POWER_CTRL_PORT,SCREEN_POWER_CTRL_PIN);      /*  低电平屏幕上电                */
    Debug(SCREEN_DEBUG, "Turn On Screen Power."endl);
}

/*
    函数功能：关闭屏幕电源
*/
__STATIC_INLINE void PowerOff_Screen(void) {
    GPIO_SetBits(SCREEN_POWER_CTRL_PORT,SCREEN_POWER_CTRL_PIN);        /*  高电平屏幕断电                */
    Debug(SCREEN_DEBUG, "Turn Off Screen Power."endl);
}

/*
    打开 / 关闭屏幕电源
*/
static void ShellCallback_ScreenPower (char *arg) {
    if((arg[0] != '-') || ((arg[1] != 'n') && (arg[1] != 'f'))) {      /*  参数有效性检测                */
        Debug(SCREEN_DEBUG, "%s", ErrArgument);
        return;
    }
    
    if (arg[1] == 'n') {
        PowerOn_Screen();
    } else {
        PowerOff_Screen();
    }
}

