/*
    双向链表
*/

#include "list.h"

/*
    节点初始化
*/
void nodeInit(tNode *node) {
    if (node == (tNode*)0) {
        return;
    }
    node->prevNode = node;
    node->nextNode = node;
}

/*
    链表初始化
*/
void listInit(tList *list) {
    if (list == (tList*)0) {
        return;
    }
    nodeInit(&(list->headNode));
    list->listCount = 0;
}

/*
    获取链表节点数量
*/
int32_t listGetCount(tList *list) {
    if (list == (tList*)0) {
        return -1;
    }
    return list->listCount;
}

/*
    获取链表第一个节点
*/
tNode* listGetFirst(tList *list) {
    tNode* node = (tNode*)0;

    if ((list != (tList*)0) && (list->listCount > 0)) {
        node = list->headNode.nextNode;
    }
    
    return node;
}

/*
    获取链表最后一个节点
*/
tNode* listGetLast(tList *list) {
    tNode* node = (tNode*)0;
    
    if ((list != (tList*)0) && (list->listCount > 0)) {
        node = list->headNode.prevNode;
    }
    
    return node;
}

/*
    获取链表中当前节点的下一个节点
*/
tNode* listGetNext(tList *list, tNode *node) {
    tNode* next = (tNode*)0;
    
    if ((list == (tList*)0) || (node == (tNode*)0)) {
        return next;
    }
    
    if (node->nextNode != &(list->headNode)) {
        next = node->nextNode;
    }
    
    return next;
}

/*
    删除链表第一个节点
*/
tNode* listRemoveFirst(tList* list) {
    tNode* node = (tNode*)0;
    
    if ((list != (tList*)0) && (list->listCount > 0)) {
        node                     = list->headNode.nextNode;   
        list->headNode.nextNode  = node->nextNode;
        node->nextNode->prevNode = node->prevNode;
        
        list->listCount--;
    }
    
    return node;
}

/*
    删除链表最后一个节点
*/
tNode* listRemoveLast(tList* list) {
    tNode* node = (tNode*)0;
    
    if ((list != (tList*)0) && (list->listCount > 0)) {
        node                     = list->headNode.prevNode;
        list->headNode.prevNode  = node->prevNode;
        node->prevNode->nextNode = node->nextNode;
        
        list->listCount--;
    }
    
    return node;
}

/*
    删除链表所有节点
*/
void listRemoveAll(tList* list) {
    uint32_t count;
    
    if (list == (tList*)0) {
        return;
    }
    tNode* currNode = (tNode*)0;
    tNode* nextNode = (tNode*)0;
    
    nextNode = list->headNode.nextNode;
    for (count = list->listCount; count > 0; count--) {
        currNode           = nextNode;
        nextNode           = nextNode->nextNode;
        currNode->nextNode = currNode;
        currNode->prevNode = currNode;
    }
    
    list->headNode.nextNode = &(list->headNode);
    list->headNode.prevNode = &(list->headNode);
    list->listCount         = 0;
}

/*
    向链表头添加一个节点
*/
void listAddFirst(tList* list, tNode* node) {
    if ((node == (tNode*)0) || (list == (tList*)0)) {
        return;
    }
    node->nextNode           = list->headNode.nextNode;
    node->prevNode           = &(list->headNode);
    node->nextNode->prevNode = node;
    node->prevNode->nextNode = node;
    
    list->listCount++;
}

/*
    向链表尾添加一个节点
*/
void listAddLast(tList* list, tNode* node) {
    if ((node == (tNode*)0) || (list == (tList*)0)) {
        return;
    }
    node->nextNode           = &(list->headNode);
    node->prevNode           = list->headNode.prevNode;
    node->nextNode->prevNode = node;
    node->prevNode->nextNode = node;
    
    list->listCount++;
}
