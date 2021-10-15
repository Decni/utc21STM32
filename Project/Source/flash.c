#include "absacc.h"
#include "list.h"
#include "memory.h"
#include "spi.h"
#include "flash.h"

uint8_t FlashConfig[2048]    __at(0x0807E000);                         /*  页252 0x0807E000 - 0x0807E7FF */ 
uint8_t FlashTimestamp[2048] __at(0x0807E800);                         /*  页253 0x0807E800 - 0x0807EFFF */
//uint8_t FlashConfig[2048]    __at(0x0807F000);                         /*  页254 0x0807F000 - 0x0807F7FF */ 
//uint8_t FlashTimestamp[2048] __at(0x0807F8000);                        /*  页255 0x0807F800 - 0x0807FFFF */ 

const char MemHead[4] = {'H','E','A','D'}; 
const char MemEnd[4]  = {'E','N','D',' '};

const uint32_t* const MemHead_Ptr = (uint32_t *)MemHead;
const uint32_t* const MemEnd_Ptr  = (uint32_t *)MemEnd;

/*
    从flash中读取半个字（2Byte）
*/
#if 0
__STATIC_INLINE uint16_t Flash_ReadHalfWord(uint32_t rAddress)
{
	return *(uint16_t *)rAddress;
}
#endif

/*
    从flash中读取一个字（4Byte）
*/
__STATIC_INLINE uint32_t Flash_ReadWord(uint32_t rAddress)
{
	return *(uint32_t *)rAddress;
}

/*
    将链表中存储的时间戳保存到flash中
*/
static bool Flash_TimestampSave(void)
{
	uint32_t      wAddress;
	tTriDataNode *pDataNode;
    tNode        *pNode = (tNode*)0;
    
	if(listGetCount(TriList) <= 0) {
        return false;
    }
    
	FLASH_Status flashstate = FLASH_COMPLETE;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); 

    wAddress   = (uint32_t)FlashTimestamp;
    flashstate = FLASH_ErasePage(wAddress);
    
    if(flashstate == FLASH_COMPLETE)
    {
        flashstate  = FLASH_ProgramWord(wAddress,*MemHead_Ptr);
        wAddress   += 4;
    }
    
    if (flashstate == FLASH_COMPLETE) {
        pNode = listGetFirst(TriList);
    }
    
    while(pNode != (tNode*)0)
    {
        pDataNode = getNodeParent(tTriDataNode, node, pNode);
        
        flashstate = FLASH_ProgramWord(wAddress,pDataNode->date);
        wAddress += 4;
        if(flashstate == FLASH_COMPLETE)
        {
            flashstate  = FLASH_ProgramWord(wAddress,pDataNode->time.data);
            wAddress   += 4;
            
        } else {

            break;
        }            

        if (flashstate == FLASH_COMPLETE) {
            pNode = listGetNext(TriList, pNode);
        } else {
            break;
        }
    }
    
    if(flashstate == FLASH_COMPLETE)
    {
        flashstate  = FLASH_ProgramWord(wAddress,*MemEnd_Ptr);
        wAddress   += 4;
    }

	FLASH_Lock();
    
	if(flashstate != FLASH_COMPLETE)
		return false;
	else
		return true;
}

/*
    读取保存在flash中的时间戳，存储到链表中
*/
static bool Flash_TimestampRead(void)
{
	uint32_t     rAddress,tmpWord;
	tTriDataNode *pNode;
    
    rAddress = (uint32_t)FlashTimestamp;
    tmpWord  = Flash_ReadWord(rAddress);
    
    if(tmpWord != *MemHead_Ptr) return false;

    rAddress += 4;
    tmpWord   = Flash_ReadWord(rAddress);
    
    while(tmpWord != *MemEnd_Ptr)
    {
        pNode = (tTriDataNode*)memGet(TriMem);
        if (pNode == (tTriDataNode*)0) {
            
        }
        pNode->date = tmpWord;

        rAddress         += 4;
        tmpWord           = Flash_ReadWord(rAddress);
        pNode->time.data  = tmpWord;
        
        listAddLast(TriList, (tNode*)pNode);
        
        rAddress += 4;
        tmpWord   = Flash_ReadWord(rAddress);
    }
    
    return true;
}

/*
    将设置参数保存到flash中
*/
static bool Flash_ConfigSave(void)
{
	uint32_t wAddress;
    uint32_t *pData;
	
    pData    = (uint32_t*)(&Config);
	wAddress = (uint32_t)FlashConfig;

	FLASH_Status flashstate = FLASH_COMPLETE;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); 
	flashstate = FLASH_ErasePage(wAddress);
    
	if(flashstate == FLASH_COMPLETE)
	{
		flashstate = FLASH_ProgramWord(wAddress,*MemHead_Ptr);
	}
    
    for (int i = sizeof(tConfig) / sizeof(uint32_t); i > 0; i--) {
        if(flashstate == FLASH_COMPLETE)
        {
            wAddress   += 4;
            flashstate  = FLASH_ProgramWord(wAddress,*pData);
        } else {
            break;
        }
        
        pData++;
    }
        
	if(flashstate == FLASH_COMPLETE)
	{
        wAddress   += 4;
		flashstate  = FLASH_ProgramWord(wAddress,*MemEnd_Ptr);
	}
	FLASH_Lock();
    
	if(flashstate != FLASH_COMPLETE)
		return false;
	else
		return true;
}

/*
    读取保存在flash中的设置参数
*/
static bool Flash_ConfigRead(void)
{
	uint32_t rAddress,tmpWord;
    uint32_t *pData = (uint32_t*)&Config;
	
	rAddress = (uint32_t)FlashConfig;
	tmpWord  = Flash_ReadWord(rAddress);
	
	if(tmpWord != *MemHead_Ptr) return false;

	rAddress += 4;
	tmpWord   = Flash_ReadWord(rAddress);
    
	while(tmpWord != *MemEnd_Ptr)
	{
        *pData = tmpWord;
        
		rAddress += 4;
		tmpWord   = Flash_ReadWord(rAddress);
        
        pData++;
	}
	
	return true;
}

/*
函数功能：擦除指定的Flash页
*/
static bool Flash_ErasePage(uint32_t eAddress)
{
	FLASH_Status flashstate = FLASH_COMPLETE;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); 
	flashstate = FLASH_ErasePage(eAddress);
	FLASH_Lock();
	if(flashstate != FLASH_COMPLETE)
		return false;
	else
		return true;
}

/*
    完成指定的flash操作
*/
bool FlashOperate(tFlashOperate flashoperate)
{
	bool flashOpState = true;
    
	switch(flashoperate)
	{
		case FlashOp_ConfigSave:
			Flash_ConfigSave();
			break;
        
		case FlashOp_ConfigRead:
			Flash_ConfigRead();
			break;

		case FlashOp_TimestampSave:
			Flash_TimestampSave();
			break;
        
		case FlashOp_TimestampRead:
			Flash_TimestampRead();
			break;
        
		case FlashOp_TimestampEraser:
			Flash_ErasePage((uint32_t)FlashTimestamp);
			break;
        
		default:
			break;
	}
	return flashOpState;
}
