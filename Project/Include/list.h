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

void nodeInit(tNode *node);                                            /*  节点初始化                    */
void listInit(tList *list);                                            /*  链表初始化                    */
int32_t listGetCount(tList *list);                                     /*  获取链表节点数量              */
tNode* listGetFirst(tList *list);                                      /*  获取链表第一个节点            */
tNode* listGetLast(tList *list);                                       /*  获取链表最后一个节点          */
tNode* listGetNext(tList *list, tNode *node);                          /*  获取链表中当前节点的下一个节点*/ 
tNode* listGetPrev(tList *list, tNode *node);
tNode* listRemoveFirst(tList* list);                                   /*  删除链表第一个节点            */
tNode* listRemoveLast(tList* list);                                    /*  删除链表最后一个节点          */
void listRemoveAll(tList* list);                                       /*  删除链表所有节点              */
void listAddFirst(tList* list, tNode* node);                           /*  向链表头添加一个节点          */
void listAddLast(tList* list, tNode* node);                            /*  向链表尾添加一个节点          */

#endif
