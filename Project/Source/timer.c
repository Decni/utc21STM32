#include "stm32f10x.h"
#include "screen.h"
#include "shell.h"

/*
    ��ʱ��2��ʼ������
*/
void TimerInit(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	
	TIM_TimeBaseInitStruct.TIM_Period        = 99;
	TIM_TimeBaseInitStruct.TIM_Prescaler     = 71;
	TIM_TimeBaseInitStruct.TIM_CounterMode   = TIM_CounterMode_Up;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = 0;
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStruct);                    /*  0.1ms                         */
	
	NVIC_InitStruct.NVIC_IRQChannel                   = TIM2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 2;
	NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
	
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);
	TIM_Cmd(TIM2,ENABLE);
    
    ShellPakaged("Timer2 Initialization is Complete!\r\n");
}

/*
    ��ʱ��2�жϷ�����
*/
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2,TIM_IT_Update))
	{
		TIM_ClearITPendingBit(TIM2,TIM_IT_Update);                     /*  ����жϱ�־                  */
        TimerProcess_SPI();
        TimeoutProcess_Screen();
    }
}
