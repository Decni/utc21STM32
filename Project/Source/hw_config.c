/**
  ******************************************************************************
  * @file    hw_config.c
  * @author  MCD Application Team
  * @version V4.0.0
  * @date    21-January-2013
  * @brief   Hardware Configuration & Setup
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "stm32_it.h"
#include "hw_config.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
ErrorStatus HSEStartUpStatus;
EXTI_InitTypeDef EXTI_InitStructure;
extern __IO uint32_t CDC_packet_sent;
extern __IO uint8_t Send_Buffer[VIRTUAL_COM_PORT_DATA_SIZE] ;
extern __IO  uint32_t packet_receive;
extern __IO uint8_t Receive_length;
extern uint16_t CDC_STO;

uint8_t Receive_Buffer[64];

uint8_t CDC_SendBuffer[200];
uint16_t CDC_Send_length;
uint16_t CDC_Send_Cnt = 0;
bool CDC_ConnectFlag = true;
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);
/* Extern variables ----------------------------------------------------------*/

extern LINE_CODING linecoding;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
* Function Name  : Set_System
* Description    : Configures Main system clocks & power
* Input          : None.
* Return         : None.
*******************************************************************************/
void Set_System(void)
{
  GPIO_InitTypeDef GPIO_InitStructure; 
  /*!< At this stage the microcontroller clock setting is already configured, 
       this is done through SystemInit() function which is called from startup
       file (startup_stm32f10x_xx.s) before to branch to application main.
       To reconfigure the default setting of SystemInit() function, refer to
       system_stm32f10x.c file
     */    
   
 /* Enable USB_DISCONNECT GPIO clock   PA15->JTDI*/
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIO_DISCONNECT | RCC_APB2Periph_AFIO,ENABLE);

  /* Configure USB pull-up pin */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);
  GPIO_InitStructure.GPIO_Pin = USB_DISCONNECT_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(USB_DISCONNECT, &GPIO_InitStructure);

	//ͨ�����VBUS��ƽ���ж�USB�Ƿ�����
	GPIO_InitStructure.GPIO_Pin = USB_POWER_SAMPLING_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(USB_POWER_SAMPLING_PORT, &GPIO_InitStructure);
 
   /* Configure the EXTI line 18 connected internally to the USB IP */
  EXTI_ClearITPendingBit(EXTI_Line18);
  EXTI_InitStructure.EXTI_Line = EXTI_Line18;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);
}

/*******************************************************************************
* Function Name  : Set_USBClock
* Description    : Configures USB Clock input (48MHz)
* Input          : None.
* Return         : None.
*******************************************************************************/
void Set_USBClock(void)
{
  /* Select USBCLK source */
  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);  
  /* Enable the USB clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);

}

/*******************************************************************************
* Function Name  : Enter_LowPowerMode
* Description    : Power-off system clocks and power while entering suspend mode
* Input          : None.
* Return         : None.
*******************************************************************************/
void Enter_LowPowerMode(void)
{
  /* Set the device state to suspend */
  bDeviceState = SUSPENDED;
}

/*******************************************************************************
* Function Name  : Leave_LowPowerMode
* Description    : Restores system clocks and power while exiting suspend mode
* Input          : None.
* Return         : None.
*******************************************************************************/
void Leave_LowPowerMode(void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  /* Set the device state to the correct state */
  if (pInfo->Current_Configuration != 0)
  {
    /* Device configured */
    bDeviceState = CONFIGURED;
		CDC_Receive_DATA();
  }
  else
  {
    bDeviceState = ATTACHED;
  }
    /*Enable SystemCoreClock*/
//  SystemInit();
}

/*******************************************************************************
* Function Name  : USB_Interrupts_Config
* Description    : Configures the USB interrupts
* Input          : None.
* Return         : None.
*******************************************************************************/
void USB_Interrupts_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

  /* 2 bit for pre-emption priority, 2 bits for subpriority */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
  /* Enable the USB interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  
  /* Enable the USB Wake-up interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_Init(&NVIC_InitStructure);   
}

/*******************************************************************************
* Function Name  : USB_Cable_Config
* Description    : Software Connection/Disconnection of USB Cable
* Input          : None.
* Return         : Status
*******************************************************************************/
void USB_Cable_Config (FunctionalState NewState)
{
  if (NewState != DISABLE)
  {
    GPIO_ResetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
  }
  else
  {
    GPIO_SetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
  }
}

/*******************************************************************************
* Function Name  : Get_SerialNum.
* Description    : Create the serial number string descriptor.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Get_SerialNum(void)
{
  uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

  Device_Serial0 = *(uint32_t*)ID1;
  Device_Serial1 = *(uint32_t*)ID2;
  Device_Serial2 = *(uint32_t*)ID3;
 
  Device_Serial0 += Device_Serial2;

  if (Device_Serial0 != 0)
  {
    IntToUnicode (Device_Serial0, &Virtual_Com_Port_StringSerial[2] , 8);
    IntToUnicode (Device_Serial1, &Virtual_Com_Port_StringSerial[18], 4);
  }
}

/*******************************************************************************
* Function Name  : HexToChar.
* Description    : Convert Hex 32Bits value into char.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
  uint8_t idx = 0;
  
  for( idx = 0 ; idx < len ; idx ++)
  {
    if( ((value >> 28)) < 0xA )
    {
      pbuf[ 2* idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2* idx] = (value >> 28) + 'A' - 10; 
    }
    
    value = value << 4;
    
    pbuf[ 2* idx + 1] = 0;
  }
}

/*******************************************************************************
* Function Name  : Send DATA .
* Description    : send the data received from the STM32 to the PC through USB  
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
uint32_t CDC_Send_DATA (uint8_t *ptrBuffer, uint8_t Send_length)
{
	if(Send_length <= 0) return 0;
	/*Sent flag*/
	CDC_packet_sent = 0;
	CDC_STO = 0;
  /*if max buffer is Not reached*/
  if(Send_length < VIRTUAL_COM_PORT_DATA_SIZE)     
  {
    
    
    /* send  packet to PMA*/
    UserToPMABufferCopy((unsigned char*)ptrBuffer, ENDP1_TXADDR, Send_length);
    SetEPTxCount(ENDP1, Send_length);
    SetEPTxValid(ENDP1);
		CDC_Send_Cnt = 0;
		TargetLaserT_Ready = false;
  }
  else
  {
    /* send  packet to PMA*/
    UserToPMABufferCopy((unsigned char*)ptrBuffer, ENDP1_TXADDR, 63);
    SetEPTxCount(ENDP1, 63);
    SetEPTxValid(ENDP1);
		CDC_Send_Cnt++;
  } 
  return 1;
}

/*******************************************************************************
* Function Name  : Receive DATA .
* Description    : receive the data from the PC to STM32 and send it through USB
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
uint32_t CDC_Receive_DATA(void)
{ 
  /*Receive flag*/
  packet_receive = 0;
  SetEPRxValid(ENDP3); 
  return 1 ;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

uint32_t CDC_Packet_TargetLaserT(void)
{
	double temperature_KDx = 0;
	uint16_t skyluminance_KDx = 0;
	uint8_t address_KDx = 0;
	CDC_Send_length = 0;
	
	if(ScreenState.TargetInfomation.KD1_Ptr != NULL)
	{
		address_KDx = ScreenState.TargetInfomation.KD1_Ptr->targetAddress;
		temperature_KDx = ScreenState.TargetInfomation.KD1_Ptr->targetLaserTemperature * 0.03125;
		skyluminance_KDx = ScreenState.TargetInfomation.KD1_Ptr->targetBackgroundLuminance;
		CDC_Send_length = sprintf((char*)CDC_SendBuffer,"��1(%X):[LaserT:%10.5f�� SkyLum:%4u]\r\n",address_KDx,temperature_KDx,skyluminance_KDx);
	}
		
	if(ScreenState.TargetInfomation.KD2_Ptr != NULL)
	{
		address_KDx = ScreenState.TargetInfomation.KD2_Ptr->targetAddress;
		temperature_KDx = ScreenState.TargetInfomation.KD2_Ptr->targetLaserTemperature * 0.03125;
		skyluminance_KDx = ScreenState.TargetInfomation.KD2_Ptr->targetBackgroundLuminance;
		CDC_Send_length += sprintf((char*)&CDC_SendBuffer[CDC_Send_length],"��2(%X):[LaserT:%10.5f�� SkyLum:%4u]\r\n",address_KDx,temperature_KDx,skyluminance_KDx);
	}
	
	if(ScreenState.TargetInfomation.KD3_Ptr != NULL)
	{
		address_KDx = ScreenState.TargetInfomation.KD3_Ptr->targetAddress;
		temperature_KDx = ScreenState.TargetInfomation.KD3_Ptr->targetLaserTemperature * 0.03125;
		skyluminance_KDx = ScreenState.TargetInfomation.KD3_Ptr->targetBackgroundLuminance;
		CDC_Send_length += sprintf((char*)&CDC_SendBuffer[CDC_Send_length],"��3(%X):[LaserT:%10.5f�� SkyLum:%4u]\r\n",address_KDx,temperature_KDx,skyluminance_KDx);
	}
	
	if(ScreenState.TargetInfomation.KD4_Ptr != NULL)
	{
		address_KDx = ScreenState.TargetInfomation.KD4_Ptr->targetAddress;
		temperature_KDx = ScreenState.TargetInfomation.KD4_Ptr->targetLaserTemperature * 0.03125;
		skyluminance_KDx = ScreenState.TargetInfomation.KD4_Ptr->targetBackgroundLuminance;
		CDC_Send_length += sprintf((char*)&CDC_SendBuffer[CDC_Send_length],"��4(%X):[LaserT:%10.5f�� SkyLum:%4u]\r\n",address_KDx,temperature_KDx,skyluminance_KDx);
	}
	
	return 1;
}

