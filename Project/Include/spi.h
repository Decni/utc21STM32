#ifndef __SPI_H
#define __SPI_H

#include "list.h"
#include "memory.h"

#define SPI_TX_MAX_BYTE       5
#define SPI_TX_MAX_ITEM      32
#define SPI_RX_MAX_BYTE       8
#define SPI_RX_MAX_ITEM      20

#define SAVE_RECORD_TIME  25000                                        /*  触发 2.5s 后保存数据到 flash  */
#define FLUSH_SCREEN_TIME  2100                                        /*  触发后 210ms 刷新屏幕         */
#define RST_FPGA_WAITING   5000                                        /*  u16 上电 500ms 复位 fpga      */

#define FPGA_CNT_UNITS_ns     5                                        /*  FPGA 计数器单位为 5 ns        */
#define HUMEN_SET_UNITS_ns   10                                        /*  设置步进量为 10 ns            */
#define CONFIG_FACTOR       (HUMEN_SET_UNITS_ns / FPGA_CNT_UNITS_ns)
typedef struct _spiTxNode {
    tNode   node;
    uint8_t buff[SPI_TX_MAX_BYTE];
}tSpiTxNode;

typedef struct _spiRxNode {
    tNode   node;
    uint8_t buff[SPI_RX_MAX_BYTE];
}tSpiRxNode;

typedef union _chData{
    uint32_t data;
    struct {
        uint32_t tim : 28;
        uint32_t num :  4;
    }item;
}tChData;

typedef struct _tirDataNode {
    tNode    node;
    uint32_t date;                                                     /*  年-月-日 时-分-秒             */
    tChData  time;                                                     /*  通道号 毫秒-微秒-纳秒         */
}tTriDataNode;

typedef struct _configData {
    uint32_t po1;
    uint32_t po2;
    uint32_t po3;
    uint32_t po4;
    
    uint32_t eo1;
    uint32_t eo2;
    uint32_t eo3;
    uint32_t eo4;
    uint32_t eo5;
    uint32_t eo6;
    uint32_t eo7;
    uint32_t eo8;                                                      /*  每个输出通道的延迟时间        */
    
    uint32_t triDelay;                                                 /*  定时触发的时间                */
}tConfig;

typedef enum _msgMask {
    Mask_triDelay =  1,
    Mask_EO1      =  2,
    Mask_EO2      =  3,
    Mask_EO3      =  4,
    Mask_EO4      =  5,
    Mask_EO5      =  6,
    Mask_EO6      =  7,
    Mask_EO7      =  8,
    Mask_EO8      =  9,
    Mask_PO1      = 10,
    Mask_PO2      = 11,
    Mask_PO3      = 12,
    Mask_PO4      = 13,
    Mask_GPS      = 14,
    Mask_GPSB     = 15,
}tMsgMask;

typedef enum _mode {
    Mode_DelayGo,
    Mode_Stop,
    Mode_Run,
    Mode_DelayTri,
    Mode_SyncTri,
    Mode_Reset,
    Mode_Receve,
    Mode_Transmit,
}tMode;

typedef enum _spiState {
    SPI_IDLE,
    SPI_TRANSMIT,
    SPI_RECEVE,
    SPI_RECEVE_DONE,
    SPI_WAIT_FREE,
    
}tSpiState;

typedef enum _fpgaState {
    FPGA_UNKNOW,
    FPGA_RESET,
    FPGA_TIMEINIT,
    FPGA_WORK,
    FPGA_TRIG,
    FPGA_TRIG_DONE,
}tFpgaState;

extern tList     *TriList;
extern tMemory   *TriMem;
extern tConfig    Config;
extern tFpgaState FPGA_State;
extern uint32_t   SpiTimer_tCntR;

void ComInit_SPI(void);                                                /*  SPI初始化函数                 */
void SpiReceve (void);                                                 /*  SPI接收函数                   */
void SpiTransmit (void);                                               /*  SPI发送函数                   */
void SpiPackaged (tMsgMask mask, uint32_t value);                      /*  打包要发送的数据              */
void NotifyFPGA(tMode mode);                                           /*  通知fpga                      */ 
void TimerProcess_SPI(void);

#endif
