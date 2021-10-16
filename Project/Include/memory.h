#ifndef __MEMORY_H
#define __MEMORY_H

#include "list.h"

typedef struct _memory {
    const char *name;
    void*       memStart;
    uint32_t    blockSize;
    uint32_t    blockCount;
    uint32_t    minCount;
    tList       blockList;
    tNode       node;
}tMemory;

typedef struct _memInfo {
    const char *name;
    uint32_t    freeCount;
    uint32_t    maxCount;                                              /*  最少剩于内存块数              */
    uint32_t    blockSize;
    uint32_t    blockCount;
}tMemInfo;

/*
    对外接口
*/
extern const char *MemHelp;

void memInit(tMemory *mem, uint8_t* memStart, uint32_t blockSize,
            uint32_t blockCount, const char *memName);                 /*  初始化内存块                  */
void* memGet(tMemory* mem);                                            /*  申请内存块                    */
void memFree(tMemory *mem, void* bStart);                              /*  释放内存块                    */
void memFreeList (tMemory *mem, tList *list);                          /*  删除并释放整个链表            */
void MemListInit(void);                                                /*  MemoryList 初始化 要最先初始化*/
void ShellCallback_Memory (char *arg);                                 /*  查看内存使用情况 Shell 回调   */
#endif
