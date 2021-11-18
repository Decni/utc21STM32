#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"

#define GPS_RX_MAX_BYTE 150

typedef struct _gpsReceve {
    char    buff[GPS_RX_MAX_BYTE];
    uint8_t size;
}tGpsReceve;
// $GNRMC,092215.000,A,3409.26863,N,10858.57848,E,0.00,0.00,151121,,,A,V*05
typedef struct _gpsPostion {
    uint8_t  latD;
    float    latM;
    char     ulat;                                                     /*  N -> 北 S -> 南               */
    uint8_t  lonD;
    float    lonM;
    char     ulon;                                                     /*  E -> 东 W -> 西               */
    uint8_t  valid;
}tGpsPosition;

void ComInit_GPS (uint32_t USART_BaudRate);                            /*GPS串口初始化函数               */
void GpsProcess (void);                                                /*GPS数据处理函数                 */
#endif 
