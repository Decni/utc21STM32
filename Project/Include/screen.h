#ifndef __SCREEN_H
#define __SCREEN_H

#include "list.h"
#include "spi.h"
#include <stdbool.h>

#define SCREEN_TX_MAX_BYTE    64
#define SCREEN_TX_MAX_ITEM    32
#define SCREEN_RX_MAX_BYTE    28
#define SCREEN_RX_MAX_ITEM    4
 
#define TX_WAITING_TIME       1000                                     /*  u16 两条指令间隔 100ms        */
#define POWER_WAITING_TIME    1000                                     /*  u16 100ms                     */
#define POWER_RESET_TIME      10000                                    /*  u16 1s                        */

typedef struct _screenTxNode {
    tNode   node;
    uint8_t buff[SCREEN_TX_MAX_BYTE - 1];
    uint8_t msgCnt;
}tScreenTxNode;

typedef struct _screenRxNode {
    tNode   node;
    uint8_t buff[SCREEN_RX_MAX_BYTE - 1];
    uint8_t msgCnt;
}tScreenRxNode;

typedef void (*pvFun)(void*);

typedef struct _screenMsg {
    pvFun Reset;
    pvFun wTriChannel;
    pvFun cTriChannel;
    pvFun wTriDate;
    pvFun cTriDate;
    pvFun wTriTime;
    pvFun cTriTime;
    pvFun wTriDelay;
    pvFun cTriDelay;
    pvFun wTriRecord;
    pvFun cTriRecord;
    pvFun wRecord;
    pvFun cRecord;
    pvFun toOPTION;
    pvFun wPO1;
    pvFun wPO2;
    pvFun wPO3;
    pvFun wPO4;
    pvFun wEO1;
    pvFun wEO2;
    pvFun wEO3;
    pvFun wEO4;
    pvFun wEO5;
    pvFun wEO6;
    pvFun wEO7;
    pvFun wEO8;
    pvFun wSetBatch;
    pvFun rRtc;
    pvFun wRtc;
    pvFun wTriBatch;
    pvFun wNrToTest;
    pvFun wNrToRecord;
}tScreenMsg;


typedef enum {
	ScreenComState_TransmitIdle = 1,  //发送空闲
	ScreenComState_Transmitting,      //正在发送
	ScreenComState_TransmitWaiting,   //发送完成，两条指令要有间隔
	ScreenComState_ReceiveIdle,       //接收空闲
	ScreenComState_ReceiveCorrect     //正确接收
}tScreenComState;

typedef enum _screenId {
    UNKNOW  = -1,
    SPLASH  =  0,
    OPTION  =  1,
    SETTING =  2,
    TESTING =  3,
    RECORD  =  4,
}tScreenId;

typedef struct _screen {
    bool       Power;
    tScreenId  ID;
    tMode      TriMode;
    tMode      RunMode;
    tList     *TriList;
    tConfig   *Config;
}tScreen;

/*
    外部调用接口
*/
extern tScreen    screenInfo;
extern tScreenMsg screenMsg;

void ComInit_Screen(uint32_t USART_BaudRate);
void TimeoutProcess_Screen(void);
void ScreenProcess(void);
void ScreenReceve (void);
void ScreenTransmit (void);

#endif
