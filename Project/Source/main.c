#include "stm32f10x.h"
#include "spi.h"
#include "screen.h"
#include "gps.h"
#include "flash.h"
#include "timer.h"
#include "shell.h"
#include <time.h>

int main (void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    MemListInit();
    
    ComInit_Shell(115200);
    ComInit_SPI();
    ComInit_Screen(115200);
    ComInit_GPS(115200);
    TimerInit();
    
    FlashOperate(FlashOp_ConfigRead);
    FlashOperate(FlashOp_TimestampRead);
    
    ShellSplash();
    while (1) {
        SpiReceve();
        ScreenReceve();
        ShellReceve();
        GpsProcess();
        ScreenProcess();
        ShellProcess();
        SpiTransmit();
        ScreenTransmit();
        ShellTransmit();
    }
}
