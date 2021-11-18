#ifndef __LIST_H
#define __LIST_H

#include "stdint.h"

#define getNodeParent(parent, name, node) (parent*)((uint32_t)node - (uint32_t)&(((parent*)0)->name))

typedef struct _Node {
    struct _Node* prevNode;
    struct _Node* nextNode;
}tNode;

typedef struct _List {
    tNode headNode;
    uint32_t listCount;
}tList;

void nodeInit(tNode *node);                                            /*  �ڵ��ʼ��                    */
void listInit(tList *list);                                            /*  �����ʼ��                    */
int32_t listGetCount(tList *list);                                     /*  ��ȡ����ڵ�����              */
tNode* listGetFirst(tList *list);                                      /*  ��ȡ�����һ���ڵ�            */
tNode* listGetLast(tList *list);                                       /*  ��ȡ�������һ���ڵ�          */
tNode* listGetNext(tList *list, tNode *node);                          /*  ��ȡ�����е�ǰ�ڵ����һ���ڵ�*/ 
tNode* listGetPrev(tList *list, tNode *node);
tNode* listRemoveFirst(tList* list);                                   /*  ɾ�������һ���ڵ�            */
tNode* listRemoveLast(tList* list);                                    /*  ɾ���������һ���ڵ�          */
void listRemoveAll(tList* list);                                       /*  ɾ���������нڵ�              */
void listAddFirst(tList* list, tNode* node);                           /*  ������ͷ���һ���ڵ�          */
void listAddLast(tList* list, tNode* node);                            /*  ������β���һ���ڵ�          */

#endif
