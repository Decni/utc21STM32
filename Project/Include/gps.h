#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"

#define GPS_RX_MAX_BYTE 150

typedef struct _gpsReceve {
    char    buff[GPS_RX_MAX_BYTE];
    uint8_t size;
}tGpsReceve;

void ComInit_GPS (uint32_t USART_BaudRate);                            /*GPS���ڳ�ʼ������               */
void GpsProcess (void);                                                /*GPS���ݴ�����                 */
#endif 
