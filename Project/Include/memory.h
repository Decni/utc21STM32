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
    uint32_t    maxCount;                                              /*  ����ʣ���ڴ����              */
    uint32_t    blockSize;
    uint32_t    blockCount;
}tMemInfo;

/*
    ����ӿ�
*/
extern const char *MemHelp;

void memInit(tMemory *mem, uint8_t* memStart, uint32_t blockSize,
            uint32_t blockCount, const char *memName);                 /*  ��ʼ���ڴ��                  */
void* memGet(tMemory* mem);                                            /*  �����ڴ��                    */
void memFree(tMemory *mem, void* bStart);                              /*  �ͷ��ڴ��                    */
void memFreeList (tMemory *mem, tList *list);                          /*  ɾ�����ͷ���������            */
void MemListInit(void);                                                /*  MemoryList ��ʼ�� Ҫ���ȳ�ʼ��*/
void ShellCallback_Memory (char *arg);                                 /*  �鿴�ڴ�ʹ����� Shell �ص�   */
#endif
