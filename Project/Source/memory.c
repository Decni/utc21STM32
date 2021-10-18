/*
    定长内存
*/
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "shell.h"

tList MemoryList;
const char *MemHeag = "           Name  BlockSize  BlockNum  FreeNum  MaxNum  \r"endl;
//                    "    ScreenRxMem     128       255       664     576    \r" 
const char *MemHelp = "        "Blue(ms)": Memory Show."endl
                      "        [ ] Show All Memory Infomation."endl
                      "       [-h] Show ms Help."endl;
/*
    MemoryList 初始化 要最先初始化
*/
void MemListInit(void) {
    listInit(&MemoryList);
}
/*
    内存块初始化
*/
void memInit(tMemory *mem, uint8_t* memStart,   uint32_t blockSize,
                           uint32_t blockCount, const char *memName) {
    uint8_t *mStart, *mEnd;
                               
    if ((mem == (tMemory*)0) || (memStart == (uint8_t*)0) || (blockSize == 0) || (blockCount == 0)) {
        return;
    }
    if (blockSize < sizeof(tNode)) {
        return;
    }
    
    mStart = memStart;
    mEnd   = memStart + blockSize * blockCount;
    
    mem->name       = memName;
    mem->memStart   = memStart;
    mem->blockSize  = blockSize;
    mem->blockCount = blockCount;
    mem->minCount   = blockCount;
    listInit(&(mem->blockList));
    
    while (mStart < mEnd) {
        nodeInit((tNode*)mStart); 
        listAddLast(&(mem->blockList), (tNode*)mStart);
        mStart += blockSize;
    }
    
    listAddLast(&MemoryList,&(mem->node));
}

/*
    申请内存块
*/
void* memGet(tMemory* mem) {
    void     *memory = (void*)0;
    int32_t   tmpCnt;
    
    if (mem == (tMemory*)0) {
        return memory;
    }
    
    tmpCnt = listGetCount(&(mem->blockList));
    if (tmpCnt <= 0) {
        return memory;
    }
    
    memory = (void*)listRemoveFirst(&(mem->blockList));
    tmpCnt = listGetCount(&(mem->blockList));
    if (mem->minCount > tmpCnt) {
        mem->minCount = tmpCnt;
    }

    return memory;
}

/*
    释放内存块
*/
void memFree(tMemory *mem, void* bStart) {
    if ((mem == (tMemory*)0) || (bStart == (void*)0)) {
        return;
    }
    nodeInit((tNode*)bStart);
    listAddLast(&(mem->blockList), (tNode*)bStart);
}

/*  
    删除并释放整个链表
*/
void memFreeList (tMemory *mem, tList *list) {
    tNode *pNode;
    
    if ((mem == (tMemory*)0) || (list == (tList*)0) || (list->listCount == 0)) {
        return;
    }
    
    pNode = listRemoveFirst(list);
    
    while (pNode != (tNode*)0) {
        nodeInit(pNode);
        listAddLast(&(mem->blockList), pNode);
        pNode = listRemoveFirst(list);
    }
    
}

/*
    获取内存块信息
*/
static void memGetInfo(tMemory* mem, tMemInfo* info) {
    int32_t tmpCnt;
    
    if ((mem == (tMemory*)0) || (info == (tMemInfo*)0)) {
        return;
    }
    tmpCnt = listGetCount(&(mem->blockList));                          /*  listGetCount 错误时返回 -1    */
    if (tmpCnt < 0) {
        return;
    }
    info->name       = mem->name;
    info->blockSize  = mem->blockSize;
    info->blockCount = mem->blockCount;
    info->maxCount   = mem->blockCount - mem->minCount;
    info->freeCount  = tmpCnt;
}

/*
    查看内存使用情况
*/
void ShellCallback_Memory (char *arg) {
    tMemory *pMemNode;
    tNode   *pNode;
    tMemInfo memInfo;
    char     tmpBuff[128];
    uint8_t  tmpCnt;
    
    pNode = listGetFirst(&MemoryList);
    if (pNode != (tNode*)0) {
        ShellPakaged(MemHeag);
    }
   
    while (pNode != (tNode*)0) {
        pMemNode = getNodeParent(tMemory, node, pNode);
        memGetInfo(pMemNode, &memInfo);
        tmpCnt = sprintf(tmpBuff,Blue(%15s)"     %3u       %3u       %3u     %3u"endl,
        memInfo.name, memInfo.blockSize, memInfo.blockCount, memInfo.freeCount, memInfo.maxCount);
        tmpBuff[tmpCnt] = '\0';
        ShellPakaged(tmpBuff);
        
        pNode = listGetNext(&MemoryList, &(pMemNode->node));
    }
}
