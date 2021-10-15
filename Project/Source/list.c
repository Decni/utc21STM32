/*
    ˫������
*/

#include "list.h"

/*
    �ڵ��ʼ��
*/
void nodeInit(tNode *node) {
    if (node == (tNode*)0) {
        return;
    }
    node->prevNode = node;
    node->nextNode = node;
}

/*
    �����ʼ��
*/
void listInit(tList *list) {
    if (list == (tList*)0) {
        return;
    }
    nodeInit(&(list->headNode));
    list->listCount = 0;
}

/*
    ��ȡ����ڵ�����
*/
int32_t listGetCount(tList *list) {
    if (list == (tList*)0) {
        return -1;
    }
    return list->listCount;
}

/*
    ��ȡ�����һ���ڵ�
*/
tNode* listGetFirst(tList *list) {
    tNode* node = (tNode*)0;

    if ((list != (tList*)0) && (list->listCount > 0)) {
        node = list->headNode.nextNode;
    }
    
    return node;
}

/*
    ��ȡ�������һ���ڵ�
*/
tNode* listGetLast(tList *list) {
    tNode* node = (tNode*)0;
    
    if ((list != (tList*)0) && (list->listCount > 0)) {
        node = list->headNode.prevNode;
    }
    
    return node;
}

/*
    ��ȡ�����е�ǰ�ڵ����һ���ڵ�
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
    ɾ�������һ���ڵ�
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
    ɾ���������һ���ڵ�
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
    ɾ���������нڵ�
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
    ������ͷ���һ���ڵ�
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
    ������β���һ���ڵ�
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
