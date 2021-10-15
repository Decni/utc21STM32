#ifndef __RS232_H
#define __RS232_H

#include "list.h"
#include "color.h"

#define SHELL_TX_MAX_BYTE    256
#define SHELL_TX_MAX_ITEM    28
#define SHELL_RX_MAX_BYTE    64
#define SHELL_RX_MAX_ITEM    4
#define SHELL_CMD_MAX_ITEM   20
#define SHELL_ECHO_MAX_BYTE  64 

#define endl        "\r"

#define Red(str)   COLOR_START COLOR_FG_RED   COLOR_TYPE_BOLD #str COLOR_STOP
#define Green(str) COLOR_START COLOR_FG_GREEN COLOR_TYPE_BOLD #str COLOR_STOP
#define Blue(str)  COLOR_START COLOR_FG_BLUE  COLOR_TYPE_BOLD #str COLOR_STOP

typedef struct _shellTxNode {
    tNode   node;
    uint8_t buff[SHELL_TX_MAX_BYTE];
    uint8_t msgCnt;
    uint8_t msgIndex;
}tShellTxNode;

typedef struct _shellRxNode {
    tNode   node;
    uint8_t buff[SHELL_RX_MAX_BYTE];
    uint8_t msgCnt;
}tShellRxNode;

typedef struct _shellEcho {
    uint8_t buff[SHELL_ECHO_MAX_BYTE];
    uint8_t msgCnt;
    uint8_t msgIndex;
}tShellEcho;

typedef void (*tShellFun)(char*);
typedef struct _shellCmdNode {
    const char *cmd;
    const char *help;
    tShellFun   fun;
    tNode       node;
}tShellCmdNode;

typedef enum _shellComState {
    ShellCom_Idle,
    ShellCom_Transmit,
    ShellCom_Echo,
    ShellCom_Receve,
}tShellComState;

/*
    ����ӿ�
*/
extern const char *UnknowCmd;
extern const char *FewArgument;
extern const char *ErrArgument;
extern const char DELIM[];

void ComInit_Shell (uint32_t USART_BaudRate);                          /*  Shell���ڳ�ʼ������           */
//void ShellPakaged(const char* msg);                                    /*  shell�������                 */
void ShellPakaged(const char* format, ...);
void ShellTransmit(void);                                              /*  shell���ͺ���                 */
void ShellReceve(void);                                                /*  shell���պ���                 */
void ShellProcess (void);                                              /*  shell��Ϣ��������             */
void ShellCmdAdd(const char *cmd, tShellFun fun, const char *help);    /*  shellָ�����Ӻ���             */
void ShellSplash(void);                                                /*  ��ʼ�������ʾ��ӭlogo        */
#endif