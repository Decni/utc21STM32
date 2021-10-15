#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f10x.h"
#include "stdbool.h"

typedef enum
{
	FlashOp_ConfigSave,
	FlashOp_ConfigRead,
	
	FlashOp_TimestampSave,
	FlashOp_TimestampRead,
	FlashOp_TimestampEraser,

}tFlashOperate;

bool FlashOperate(tFlashOperate flashoperate);


#endif 
