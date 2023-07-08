/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  False = 0,
  True = 1
} bool;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_LENGTH 1536
#define HEAD_LENGTH 15
#define BUFFE_SIZE 256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/**************属性上报消息构造**************/
char Tx_str[BUFFE_SIZE] = "AT+HMPUB=1,\"$oc/devices/649a72522a3b1d3de71e9e81_car01/sys/properties/report\",87,\"{\\\"services\\\":[{\\\"service_id\\\":\\\"car_01\\\",\\\"properties\\\":{\\\"location\\\":[";
char Tx_FlagSet[BUFFE_SIZE] = "AT+HMPUB=1,\"$oc/devices/649a72522a3b1d3de71e9e81_car01/sys/properties/report\",66,\"{\\\"services\\\":[{\\\"service_id\\\":\\\"car_01\\\",\\\"properties\\\":{\\\"car_stop\\\":1}}]}\"\r\n";
char Tx_FlagReset[BUFFE_SIZE] = "AT+HMPUB=1,\"$oc/devices/649a72522a3b1d3de71e9e81_car01/sys/properties/report\",66,\"{\\\"services\\\":[{\\\"service_id\\\":\\\"car_01\\\",\\\"properties\\\":{\\\"car_stop\\\":0}}]}\"\r\n";
uint16_t stableTx_length = 0;
/***************命令下发响应消息构造*****************/
char responseMessage[BUFFE_SIZE] = "AT+HMPUB=1,\"$oc/devices/649a72522a3b1d3de71e9e81_car01/sys/commands/response/";
uint16_t stableResponse_length_cmd = 0;
char responseProperties[BUFFE_SIZE] = "AT+HMPUB=1,\"$oc/devices/649a72522a3b1d3de71e9e81_car01/sys/properties/set/response/";
uint16_t stableResponse_length_flag = 0;
char *strstr(const char *, const char *);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char *strx, *strx_head, *strx_end, *strx_properties;
char RxBuffer[MAX_LENGTH] = {0};
uint16_t Rxcounter = 0;
char Buffer_total[MAX_LENGTH] = {0};
uint16_t TotalCounter = 0;
bool initStatus = False;
bool UART_Frame_Flag = False;
bool StopFlag = False;
/***********json解析变量***********/
cJSON *root = NULL;
cJSON *location_array = NULL;
uint8_t location_length = 0;
uint8_t location_index = 0;
char json_str[MAX_LENGTH] = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

//释放cJSON内存，为下一次解析做准备
void Clear_cJSON()
{
  if (root)
    cJSON_Delete(root);
  root = NULL;
  memset(json_str, 0, MAX_LENGTH);
  location_array = NULL;
  location_length = 0;
  location_index = 0;
}
//清空整体接收缓存
void Clear_TotalBuffer(void)
{
  memset(Buffer_total, 0, TotalCounter + 1);
  TotalCounter = 0;
  strx = NULL;
  strx_head = strx_end = strx_properties = NULL;
}

//生成属性上报消息包
void Generate_dynamicPart(char *dest, double lng, double lat)
{
  char lng_str[12];
  char lat_str[12];
  sprintf(lng_str, "%.6f", lng);
  sprintf(lat_str, "%.6f", lat);
  strcat(dest, lng_str);
  strcat(dest, ",");
  strcat(dest, lat_str);
  strcat(dest, "]}}]}\"\r\n");
}
//获取cJSON对象
bool GetcJSON_Object()
{
  //找到json数据的指针
  char *start_ptr = strstr(Buffer_total, "{\"paras\":{\"location_array\":");
  if (!start_ptr)
    return False;
  //提取出完整的json数据字符串
  strcpy(json_str, start_ptr);
  char *end_ptr = strstr(json_str, "\"\r\n");
  for (uint8_t i = 0; i <= 3; i++)
    end_ptr[i] = '\0';
  printf("get cloud json data: \n%s\n", json_str);
  //解析json字符串
  root = cJSON_Parse(json_str);
  if (!root)
  {
    root = cJSON_Parse(json_str);
    if (!root)
    {
      printf("json parse failed!!!\n");
      return False;
    }
  }
  location_array = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "paras"), "location_array");
  printf("root address: %p, array address: %p\n", root, location_array);
  location_length = cJSON_GetArraySize(location_array);
  location_index = 0;
  return True;
}
//获取下发命令响应消息, 并响应给华为云
void ResponseCommandMessage(bool status)
{
  char *request_id = strstr(Buffer_total, "request_id=");
  char *request_end = strchr(request_id, '\"');
  uint16_t length = request_end - request_id;
  strncat(responseMessage, request_id, length);
  if (status == True)
    strcat(responseMessage, "\",17,\"{\\\"result_code\\\":0}\"\r\n");
  else
    strcat(responseMessage, "\",17,\"{\\\"result_code\\\":1}\"\r\n");
  //拼接完成响应消息后，返回给华为云
  uart_L610_send(responseMessage);
  //清理可变部分，以便下次再用
  memset(responseMessage + stableResponse_length_cmd, 0, BUFFE_SIZE - stableResponse_length_cmd);
  printf("okk2\r\n");
}
void ResponseFlagMessage(bool status)
{
  char *request_id = strstr(Buffer_total, "request_id=");
  char *request_end = strchr(request_id, '\"');
  uint16_t length = request_end - request_id;
  strncat(responseProperties, request_id, length);
  if (status == True)
    strcat(responseProperties, "\",17,\"{\\\"result_code\\\":0}\"\r\n");
  else
    strcat(responseProperties, "\",17,\"{\\\"result_code\\\":1}\"\r\n");
  //拼接完成响应消息后，返回给华为云
  uart_L610_send(responseProperties);
  //清理可变部分，以便下次再用
  memset(responseProperties + stableResponse_length_flag, 0, BUFFE_SIZE - stableResponse_length_flag);
  // printf("%s\n", responseProperties);
  printf("okk1\r\n");
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**********************串口接收空闲中断回调***********************/
void USER_UART_IRQHandler(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    //判断是否为空闲中断
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart1); //清除空闲中断标志位
      // printf("\nclear IQR...\n");
      USER_UART_IDLECallback(&huart1);
    }
  }
  if (huart->Instance == USART2)
  {
    //判断是否为空闲中断
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart2); //清除空闲中断标志位
      // printf("\nclear IQR...\n");
      USER_UART_IDLECallback(&huart2);
    }
  }
  if (huart->Instance == USART3)
  {
    //判断是否为空闲中断
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart3); //清除空闲中断标志位
      // printf("\nclear IQR...\n");
      USER_UART_IDLECallback(&huart3);
    }
  }
  if (huart->Instance == USART6)
  {
    //判断是否为空闲中断
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart6); //清除空闲中断标志位
      // printf("\nclear IQR...\n");
      USER_UART_IDLECallback(&huart6);
    }
  }
}
void USER_UART_IDLECallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    HAL_UART_DMAStop(&huart2); //停止DMA传输
    //获取数据长度
    Rxcounter = MAX_LENGTH - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    TotalCounter += Rxcounter;
    strcat(Buffer_total, RxBuffer);
    UART_Frame_Flag = True;
    // if (initStatus)
    // printf("%s", Buffer_total);
    memset(RxBuffer, 0, Rxcounter);
    HAL_UART_Receive_DMA(&huart2, (uint8_t *)RxBuffer, MAX_LENGTH);
  }
}

/*****************************定时器溢出事件回调********************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(htim);
  if (htim->Instance == TIM2)
  {
    HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
    if (location_length > 0)
    {
      HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      cJSON *location = cJSON_GetArrayItem(location_array, location_index++);
      cJSON *lng = cJSON_GetArrayItem(location, 0);
      cJSON *lat = cJSON_GetArrayItem(location, 1);
      //生成上报属性的消息
      char dynamicPart[36] = {0};
      Generate_dynamicPart(dynamicPart, lng->valuedouble, lat->valuedouble);
      location = lng = lat = NULL;
      strcpy(Tx_str + stableTx_length, dynamicPart);
      uart_L610_send(Tx_str);
      //清理Tx_str, 只清理动态变化的部分
      memset(Tx_str + stableTx_length, 0, BUFFE_SIZE - stableTx_length);
      if (location_index >= location_length)
      {
        Clear_cJSON();
        StopFlag = True;
      }
    }
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  /* USER CODE BEGIN 2 */
  //开启定时器中断
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_Base_Start_IT(&htim4);
  HAL_TIM_Base_Start_IT(&htim5);
  //开启串口接收空闲中断
  //清除IDLE标志
  //  __HAL_UART_CLEAR_IDLEFLAG(&huart1);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
  //  __HAL_UART_CLEAR_IDLEFLAG(&huart2);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
  //  __HAL_UART_CLEAR_IDLEFLAG(&huart3);
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
  //  __HAL_UART_CLEAR_IDLEFLAG(&huart6);
  __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);
  //开启DMA接收
  // HAL_UART_Receive_DMA(&huart1, (uint8_t *)RxBuffer, MAX_LENGTH);
  HAL_UART_Receive_DMA(&huart2, (uint8_t *)RxBuffer, MAX_LENGTH);
  //  HAL_UART_Receive_DMA(&huart3, (uint8_t *)RxBuffer, MAX_LENGTH);
  //  HAL_UART_Receive_DMA(&huart6, (uint8_t *)RxBuffer, MAX_LENGTH);

  /*******************************************用户初始化****************************************************/
  printf("stm32 init begin...\n");
  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);

  /****查询版本信息***/
  uart_L610_send("ATI\r\n");
  HAL_Delay(2000);
  // HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
  printf("%s", Buffer_total);
  strx = strstr((const char *)Buffer_total, "Fibocom"); //校验数据是否正确
  while (strx == NULL)
  {
    printf("Query info failed!!!\n");
    //重复申请查询
    Clear_TotalBuffer();
    uart_L610_send("ATI\r\n");
    HAL_Delay(1000);
    printf("%s", Buffer_total);
    strx = strstr((const char *)Buffer_total, "Fibocom");
  }
  printf("1.got the correct version...\n");
  HAL_Delay(500);
  Clear_TotalBuffer();

  /****请求IP****/
  uart_L610_send("AT+MIPCALL?\r\n"); //查询是否已有IP
  HAL_Delay(1000);
  printf("%s", Buffer_total);
  strx = strstr((const char *)Buffer_total, "+MIPCALL: 1");
  while (strx == NULL)
  {
    printf("got none IP now!!!\n");
    //重复申请IP
    Clear_TotalBuffer();
    uart_L610_send("AT+MIPCALL=1\r\n");
    HAL_Delay(1000);
    printf("%s", Buffer_total);
    strx = strstr((const char *)Buffer_total, "+MIPCALL: ");
  }
  printf("2.got the IP address...\n");
  HAL_Delay(500);
  Clear_TotalBuffer();

  /****连接华为云****/
  //服务地址 端口号 设备ID 设备秘钥
  uart_L610_send("AT+HMCON=0,60,\"121.36.42.100\",\"8883\",\"649a72522a3b1d3de71e9e81_car01\",\"123456789\",0\r\n");
  HAL_Delay(2500); //这一步响应时间在1s多
  printf("%s", Buffer_total);
  strx = strstr((const char *)Buffer_total, "+HMCON OK"); //检测是否成功连接
  // while (strx == NULL)
  // {
  //   printf("HuaweiCloud connected failed!!!\n");
  //   //重复申请连接华为云
  //   Clear_TotalBuffer();
  //   uart_L610_send("AT+HMCON=0,60,\"121.36.42.100\",\"8883\",\"649a72522a3b1d3de71e9e81_car01\",\"123456789\",0\r\n");
  //   HAL_Delay(2000);
  //   printf("%s", Buffer_total);
  //   strx = strstr((const char *)Buffer_total, "+HMCON OK");
  // }
  printf("3.HuaweiCloud connected successfully...\n");
  Clear_TotalBuffer();
  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
  printf("stm32 init success...\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  initStatus = True;
  UART_Frame_Flag = False;
  //获取上报属性消息固定长度
  stableTx_length = strlen(Tx_str);
  stableResponse_length_cmd = strlen(responseMessage);
  stableResponse_length_flag = strlen(responseProperties);
  //绿色状态灯亮起
  HAL_Delay(500);
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
  Clear_TotalBuffer();
  while (1)
  {
    if(StopFlag == True)
    {
      HAL_Delay(100);
      uart_L610_send(Tx_FlagSet);
      StopFlag = False;
    }
    if (UART_Frame_Flag == True)
    {
      strx_head = strstr(Buffer_total, "+HMREC");
      if(strx_head) 
        printf("%s", Buffer_total);
      strx_end = strstr(Buffer_total, "\"SendArray\"}\"\r\n");
      // printf("head address: %p, end address: %p\n", strx_head, strx_end);
      //校验下发命令的包头包尾都正确
      if (strx_head && strx_end)
      {
        // printf("%s", Buffer_total);
        Clear_cJSON();
        bool status = GetcJSON_Object();
        ResponseCommandMessage(status);
        Clear_TotalBuffer();
      }
      //收到停车状态重置命令
      if (strx_head && !strx_end)
      {
        printf("%s", Buffer_total);
        strx_properties = strstr(Buffer_total, "\"car_stop\":0}}]}\"\r\n");
        if (strx_properties)
        {
          printf("update properties...\n");
          //重置停车状态
          ResponseFlagMessage(True);
          HAL_Delay(100);
          uart_L610_send(Tx_FlagReset);
          printf("resetflag success...\n");
          Clear_TotalBuffer();
        }
      }
      if(!strx_head) Clear_TotalBuffer();
      UART_Frame_Flag = False;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
   */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
