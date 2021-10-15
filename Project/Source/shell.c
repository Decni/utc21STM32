#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "shell.h"
#include "platform_config.h"
#include "memory.h"

#define CHAR_DE     0X7F
#define CHAR_BS     '\b'
#define CHAR_SPACE  ' '
#define CHAR_CR     '\r'
#define CHAR_LF     '\n'
#define CHAR_TAB    '\t'


const char *UnknowCmd   = "Unknown Command!"endl;
const char *FewArgument = "Too Few Argument!"endl;
const char *ErrArgument = "Error Argument!"endl;
const char *HelpHelp    = "      "Blue(help)": Show Cmd Infomation."endl;
const char *ClsHelp     = "       "Blue(cls)": Clear Screen."endl;
const char *RstHelp     = "       "Blue(rst)": Reset System."endl;

// 分隔符
const char DELIM[] = {CHAR_SPACE, CHAR_CR, CHAR_LF, CHAR_TAB, '\0'};

tList   ShellTxList, ShellRxList, ShellCmdList;                        /*  shell串口收发数据链表         */
tMemory ShellTxMem, ShellRxMem, ShellCmdMem;                           /*  shell串口收发内存块           */
uint8_t ShellTxBuff[sizeof(tShellTxNode) * SHELL_TX_MAX_ITEM];
uint8_t ShellRxBuff[sizeof(tShellRxNode) * SHELL_RX_MAX_ITEM];         /*  shell串口收发缓存区           */
uint8_t ShellCmdBuff[sizeof(tShellCmdNode) * SHELL_CMD_MAX_ITEM];
                                                                       
tShellRxNode *pShellRxNodeA, *pShellRxNodeB, *pShellRxNodeCurr;        /*  shell串口接收节点指针         */
tShellTxNode *pShellTxNode;                                            /*  shell串口发送节点指针         */
tShellEcho   ShellEcho;                                                /*  回显缓冲区                    */
tShellComState ShellComState, ShellComRxState;                         /*  shell串口收发端口状态         */

static void ShellCallback_ShowHelp (char *arg);
static void Shellcallback_ClearScreen (char *arg);
void ShellCallback_SoftReset(char *arg);

/*
    Shell串口初始化函数
*/
void ComInit_Shell (uint32_t USART_BaudRate) {
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef  NVIC_InitStruct;

    listInit(&ShellTxList);                                            /*  初始化发送和接收链表          */
	listInit(&ShellRxList);
    listInit(&ShellCmdList);
    memInit(&ShellTxMem,ShellTxBuff, sizeof(tShellTxNode), SHELL_TX_MAX_ITEM, "ShellTxMem");
	memInit(&ShellRxMem,ShellRxBuff, sizeof(tShellRxNode), SHELL_RX_MAX_ITEM, "ShellRxMem");
    memInit(&ShellCmdMem,ShellCmdBuff, sizeof(tShellCmdNode), SHELL_CMD_MAX_ITEM, "ShellCmdMem");
                                                                       /*  初始化发送和接受内存块        */ 
    pShellRxNodeA    = (tShellRxNode*)memGet(&ShellTxMem);             /*  初始化接收节点                */
    pShellRxNodeB    = (tShellRxNode*)memGet(&ShellTxMem);
    pShellRxNodeCurr = pShellRxNodeA;
    
    pShellRxNodeCurr->msgCnt = 0;
    ShellEcho.msgCnt         = 0;
    
    ShellComState   = ShellCom_Idle;
    ShellComRxState = ShellCom_Idle;
    
    RCC_APB2PeriphClockCmd(Shell_COM_RX_CLK | Shell_COM_TX_CLK | RCC_APB2Periph_AFIO,ENABLE);
    RCC_APB2PeriphClockCmd(Shell_COM_CLK,ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1,ENABLE);
    
	
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin   = Shell_COM_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(Shell_COM_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin  = Shell_COM_RX_PIN;
    GPIO_Init(Shell_COM_RX_PORT, &GPIO_InitStructure);
	
    USART_InitStruct.USART_BaudRate            = USART_BaudRate;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_Init(Shell_COM, &USART_InitStruct);
    
    NVIC_InitStruct.NVIC_IRQChannel                   = Shell_COM_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 1;
	NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
  
	USART_ClearFlag(Shell_COM,USART_FLAG_RXNE);
    USART_ClearFlag(Shell_COM,USART_FLAG_TC);
    USART_ITConfig(Shell_COM,USART_IT_RXNE,ENABLE);
	USART_ITConfig(Shell_COM,USART_IT_TC,ENABLE);                      /*  使能串口收发中断              */
	
    USART_Cmd(Shell_COM, ENABLE);
    

    ShellCmdAdd("help", ShellCallback_ShowHelp, HelpHelp);
    ShellCmdAdd("ms", ShellCallback_Memory, MemHelp);
    ShellCmdAdd("cls", Shellcallback_ClearScreen, ClsHelp);
    ShellCmdAdd("rst", ShellCallback_SoftReset, RstHelp);
    ShellPakaged("\rShell Com Initialization is Complete!"endl);  
}

/*
    shell打包函数
*/
void ShellPakaged(const char* format, ...) {
    tShellTxNode *tmpTxNode;
    va_list       var;
    
    tmpTxNode = (tShellTxNode*)memGet(&ShellTxMem);                    /*  申请一块内存                  */
    if (tmpTxNode == (tShellTxNode*)0) {
        return;
    }
    
    va_start(var, format);
    tmpTxNode->msgCnt = (uint8_t)vsprintf((char*)(tmpTxNode->buff), format, var);
    va_end(var);
    tmpTxNode->msgIndex = 0;

    listAddLast(&ShellTxList, &(tmpTxNode->node));
    tmpTxNode = (tShellTxNode*)0;

}
/*
    shell发送函数
*/
void ShellTransmit(void) {
    if (ShellComState == ShellCom_Idle) {
        if (ShellEcho.msgIndex != ShellEcho.msgCnt) {
            ShellComState = ShellCom_Echo;
            USART_SendData(Shell_COM, ShellEcho.buff[ShellEcho.msgIndex++]);
            
        } else if (listGetCount(&ShellTxList) > 0) {
            pShellTxNode  = getNodeParent(tShellTxNode, node, listRemoveFirst(&ShellTxList));
            ShellComState = ShellCom_Transmit;
            USART_SendData(Shell_COM, pShellTxNode->buff[pShellTxNode->msgIndex++]);
        }
    }
}

/*
    shell接收函数
*/
void ShellReceve(void) {
    if (ShellComRxState == ShellCom_Receve) {
        if (pShellRxNodeCurr == pShellRxNodeA) {
            if (listGetCount(&ShellRxList) < SHELL_RX_MAX_ITEM) {
                listAddLast(&ShellRxList, &(pShellRxNodeB->node));     /*  将接收到的数据插入接收链表    */
                pShellRxNodeB = (tShellRxNode*)memGet(&ShellRxMem);    /*  获取新的节点                  */
                if (pShellRxNodeB != 0) {
                    pShellRxNodeB->msgCnt = 0;
                } else {
#ifdef DEBUG
                    ShellPakaged(Red(ERROR)": %s Out Of Memory!"endl, __func__);
#endif
                }
            } else {
                pShellRxNodeB->msgCnt = 0;
            }
        } else {
            
            if (listGetCount(&ShellRxList) < SHELL_RX_MAX_ITEM) {
                listAddLast(&ShellRxList, &(pShellRxNodeA->node));         /*  将接收到的数据插入接收链表    */
                pShellRxNodeA = (tShellRxNode*)memGet(&ShellRxMem);        /*  获取新的节点                  */
                if (pShellRxNodeA != 0) {
                    pShellRxNodeA->msgCnt = 0;
                } else {
#ifdef DEBUG
                    ShellPakaged(Red(ERROR)": %s Out Of Memory!"endl, __func__);
#endif
                }
            } else {
                pShellRxNodeA->msgCnt = 0;
            }
        }
        ShellComRxState = ShellCom_Idle;
    }
}

/*
    shell消息处理函数
*/
void ShellProcess (void) {
    tShellRxNode  *tmpRxNode;
    tShellCmdNode *tmpCmdNode;
    tNode         *pNode;
    char          *token;
    
    if (listGetCount(&ShellRxList) > 0) {
        tmpRxNode = getNodeParent(tShellRxNode, node, listRemoveFirst(&ShellRxList));
        pNode     = listGetFirst(&ShellCmdList);
        token     = strtok((char*)&(tmpRxNode->buff), DELIM);


        while (pNode != (tNode*)0) {
            tmpCmdNode = getNodeParent(tShellCmdNode, node, pNode);
            
            if (strcmp(token, tmpCmdNode->cmd) == 0) {
                token = strtok(NULL, DELIM);
                if (strstr(token, "-h")) {
                    ShellPakaged(tmpCmdNode->help);
                } else if (tmpCmdNode->fun != NULL) {
                    
                    tmpCmdNode->fun(token);
                }
                
               memFree(&ShellRxMem, tmpRxNode);
               ShellPakaged(Green(KD>\040)); 
               return;
            }
            
            pNode = listGetNext(&ShellCmdList, &(tmpCmdNode->node));
        }
        
        memFree(&ShellRxMem, tmpRxNode);
        ShellPakaged(UnknowCmd);
        ShellPakaged(Green(KD>\040));
    }
}

/*
    回显字符处理
*/
void ShellEchoProcess(uint16_t ch) {
    switch (ch) {
        case CHAR_DE:                                                  /* 0x7f delete                    */
                                                                       /*  没有 break                    */
        case CHAR_BS:                                                  /*  '\b'                          */
            if (pShellRxNodeCurr->msgCnt > 0) {
                pShellRxNodeCurr->buff[--pShellRxNodeCurr->msgCnt] = '\0';
                if (ShellEcho.msgCnt < SHELL_ECHO_MAX_BYTE - 5) {
                    ShellEcho.buff[ShellEcho.msgCnt++] = CHAR_BS;
                    ShellEcho.buff[ShellEcho.msgCnt++] = CHAR_SPACE;
                    ShellEcho.buff[ShellEcho.msgCnt++] = CHAR_BS;
                }
            }
            break;
        
        case CHAR_CR:                                                  /*  '\r'                          */
                                                                       /*  没有 break                    */
        case CHAR_LF:                                                  /*  '\n'                          */
            if (pShellRxNodeCurr->msgCnt > 0) {
                ShellEcho.buff[ShellEcho.msgCnt++] = ch;
                pShellRxNodeCurr->buff[pShellRxNodeCurr->msgCnt] = '\0';
                
                if (pShellRxNodeCurr == pShellRxNodeA) {
                    if (pShellRxNodeB != (tShellRxNode*)0) {
                        pShellRxNodeCurr = pShellRxNodeB;
                    } else {
#ifdef DEBUG
                    ShellPakaged(Red(ERROR)": Shell Receve Buffer Invalid!"endl);
#endif
                    }
                } else {
                    if (pShellRxNodeA != (tShellRxNode*)0) {
                        pShellRxNodeCurr = pShellRxNodeA;
                    } else {
#ifdef DEBUG
                    ShellPakaged(Red(ERROR)": Shell Receve Buffer Invalid!"endl);
#endif
                    }
                }
                pShellRxNodeCurr->msgCnt = 0;
                ShellComRxState          = ShellCom_Receve;
            } else if (ch == '\r') {
                ShellPakaged(Green(\rKD>\040));                        /*  先换行                        */
            }
            break;
        
        default :
            if (pShellRxNodeCurr->msgCnt < SHELL_RX_MAX_BYTE - 1) {    /*  为字符串结束 '\0' 预留位置    */
                pShellRxNodeCurr->buff[pShellRxNodeCurr->msgCnt++] = ch;
            }
            
            if (ShellEcho.msgCnt < SHELL_ECHO_MAX_BYTE - 2) {          /*  为行结束 '\r' '\n' 预留位置   */
                ShellEcho.buff[ShellEcho.msgCnt++] = ch;
            }
            break;
    }
}

/*
    屏幕串口中断服务函数
*/
void Shell_COM_IRQHandler(void) {
    uint16_t ch;
    
	if(USART_GetITStatus(Shell_COM,USART_IT_RXNE)) {                   /*  接收中断                      */
        USART_ClearITPendingBit(Shell_COM,USART_IT_RXNE);
        
		ch = USART_ReceiveData(Shell_COM);
        ShellEchoProcess(ch);
    }
	
    if(USART_GetITStatus(Shell_COM,USART_IT_TC))                       /*  发送中断                      */
    {
        USART_ClearITPendingBit(Shell_COM,USART_IT_TC);
        
        if (ShellComState == ShellCom_Echo) {
            if (ShellEcho.msgIndex < ShellEcho.msgCnt) {
                USART_SendData(Shell_COM, ShellEcho.buff[ShellEcho.msgIndex++]);
            } else {
                ShellEcho.msgIndex = 0;
                ShellEcho.msgCnt   = 0;
                ShellComState = ShellCom_Idle;
            }
        }else if (ShellComState == ShellCom_Transmit) {
            
            if (pShellTxNode->msgIndex < pShellTxNode->msgCnt) {
                USART_SendData(Shell_COM, pShellTxNode->buff[pShellTxNode->msgIndex++]);
            } else {
                ShellComState = ShellCom_Idle;
                memFree(&ShellTxMem, pShellTxNode);
                pShellTxNode  = (tShellTxNode*)0;
            }
        }
    }
}

/*
    shell指令添加函数
*/
void ShellCmdAdd(const char *cmd, tShellFun fun, const char *help) {
    tShellCmdNode *pCmdNode;

    pCmdNode = (tShellCmdNode*)memGet(&ShellCmdMem);
    if (pCmdNode == (tShellCmdNode*)0) {
#ifdef DEBUG
        ShellPakaged(Red(ERROR)": %s Out of Memrmory!"endl, __func__);
#endif
        return;
    }
    pCmdNode->cmd  = cmd;
    pCmdNode->fun  = fun;
    pCmdNode->help = help;
    
    listAddLast(&ShellCmdList, &(pCmdNode->node));

    pCmdNode = (tShellCmdNode*)0;
}

/*
    显示所有指令的帮助
*/
static void ShellCallback_ShowHelp (char *arg) {
    tShellCmdNode *pCmdNode;
    tNode         *pNode;
    
    pNode = listGetFirst(&ShellCmdList);

    while (pNode) {
        pCmdNode = getNodeParent(tShellCmdNode, node, pNode);
        ShellPakaged(pCmdNode->help);
        
        pNode = listGetNext(&ShellCmdList, &(pCmdNode->node));   
    }
    
}

/*
    清屏指令
*/
static void Shellcallback_ClearScreen(char *arg) {
    ShellPakaged("\033c");
}

/*
    软复位
*/
void ShellCallback_SoftReset(char *arg) {
    __set_FAULTMASK(1);                                                /*  关闭所有中断                  */ 
    NVIC_SystemReset();                                                /*  复位                          */ 
}

/*
    初始化完成显示欢迎logo
*/
void ShellSplash(void) {
    ShellPakaged(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"endl);
    ShellPakaged("  kkkk  kkk     kkkkkk     kkk     kkkk         kkkkkkk     "endl);
    ShellPakaged("   kk  kk         kk       kk k     kk        kk       kk   "endl);
    ShellPakaged("   kk kk          kk       kk kk    kk      kk              "endl);
    ShellPakaged("   kkk            kk       kk  kk   kk      kk      kkkkk   "endl);
    ShellPakaged("   kkkkk          kk       kk   k   kk      kk         kk   "endl);
    ShellPakaged("   kk  kk         kk       kk    kk kk      kk         kk   "endl);
    ShellPakaged("   kk   kk        kk       kk     k kk        kk       kk   "endl);
    ShellPakaged("  kkkk  kkk     kkkkkk    kkkk     kkk          kkkkkkk     "endl);
    ShellPakaged("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"endl);
    ShellPakaged(Green(KD>\040));
}
