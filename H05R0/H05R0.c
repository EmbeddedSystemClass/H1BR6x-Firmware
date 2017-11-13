/*
    BitzOS (BOS) V0.1.3 - Copyright (C) 2017 Hexabitz
    All rights reserved

    File Name     : H05R0.c
    Description   : Source code for module H05R0.
										SPI-based uSD driver with Fatfs.  
		
		Required MCU resources : 
		
			>> USARTs 1,2,3,4,5 for module ports.
			>> DMA1 Ch5, DMA1 Ch6, DMA2 Ch2 for port-to-memory messaging.
			>> DMA1 Ch1, DMA1 Ch3, DMA2 Ch3 for port-to-port streaming.
			>> SPI1 for uSD
			>> PB0 for uSD detect
			
*/
	
/* Includes ------------------------------------------------------------------*/
#include "BOS.h"

/* Define UART variables */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;

/*=================================================================================*/
/*========================= Private variables  ====================================*/
/*=================================================================================*/
log_t logs[MAX_LOGS];
logVar_t logVars[MAX_LOG_VARS];
uint32_t compareValue[MAX_LOG_VARS]; 
/* File system object for SD card logical drive */
FATFS SDFatFs; 
/* SD card logical drive path */
char SDPath[4]; 
/* File objects */
FIL MyFile, tempFile;    
/* File write/read counts */
uint32_t byteswritten, bytesread;                     
char lineBuffer[100];
uint16_t  activeLogs;
TaskHandle_t LogTaskHandle = NULL;
uint8_t temp_uint8 = 0; 
/* Module settings - sequential log naming*/
bool enableSequential = false;
bool enableTimeDateHeader = false;


/*=================================================================================*/
/*========================= CONSTANT ==============================================*/
/*=================================================================================*/
const uint8_t numberMap[3] = {1, 10, 100};
const char logHeaderText1[] = "Datalog created by BOS V%d.%d.%d on %s\n";
const char logHeaderText2[] = "Log type: Rate @ %.2f Hz\n\n";
const char logHeaderText3[] = "Log type: Events\n\n";
const char logHeaderTimeDate[] = "%s %02d/%02d/%04d %02d:%02d:%02d\n";

/*=================================================================================*/
/*========================= Private function prototypes ===========================*/
/*=================================================================================*/	
void LogTask(void * argument);
uint8_t CheckLogVarEvent(uint16_t varIndex);
Module_Status OpenThisLog(uint16_t logindex, FIL *objFile);

/* Create CLI commands --------------------------------------------------------*/

portBASE_TYPE addLogCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE deleteLogCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE logVarCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE startCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE stopCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE pauseCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );
portBASE_TYPE resumeCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString );

/* CLI command structure : addlog */
const CLI_Command_Definition_t addLogCommandDefinition =
{
	( const int8_t * ) "addlog", /* The command string to type. */
	( const int8_t * ) "addlog:\r\n Add a new log file. Specifiy log name (1st par.); type (2nd par.): 'rate' or 'event'; \
rate (3rd par.): logging rate in Hz (max 1000), delimiter format (4th par.): 'space', 'tab' or 'comma'; index column format \
(5th par.): 'none', 'sample' or 'time'; and index column label text (6th par.)\r\n\r\n",
	addLogCommand, /* The function to run. */
	6 /* Six parameters are expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : logvar */
const CLI_Command_Definition_t logVarCommandDefinition =
{
	( const int8_t * ) "logvar", /* The command string to type. */
	( const int8_t * ) "logvar:\r\n Add a new log variable to an existing log (1st par.). Specify variable type (2nd and 3rd par.): \
'port digital', 'port data', 'port button', 'memory uint8', 'memory int8', 'memory uint16', 'memory int16', 'memory uint32', \
'memory int32', 'memory float' ; source (4th par.): ports 'p1'..'px', buttons 'b1'..'bx' or memory location (Flash or RAM); \
and column label text (5th par.)\r\n\r\n",
	logVarCommand, /* The function to run. */
	5 /* Five parameters are expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : deletelog */
const CLI_Command_Definition_t deleteLogCommandDefinition =
{
	( const int8_t * ) "deletelog", /* The command string to type. */
	( const int8_t * ) "deletelog:\r\n Delete a log file. Specifiy log name (1st par.) and delete options (2nd par.): 'all' or \
'keepdisk' to keep log on the uSD card\r\n\r\n",
	deleteLogCommand, /* The function to run. */
	2 /* Two parameters are expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : start */
const CLI_Command_Definition_t startCommandDefinition =
{
	( const int8_t * ) "start", /* The command string to type. */
	( const int8_t * ) "start:\r\n Start the log with log name (1st par.)\r\n\r\n",
	startCommand, /* The function to run. */
	1 /* One parameter is expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : stop */
const CLI_Command_Definition_t stopCommandDefinition =
{
	( const int8_t * ) "stop", /* The command string to type. */
	( const int8_t * ) "stop:\r\n Stop the log with log name (1st par.)\r\n\r\n",
	stopCommand, /* The function to run. */
	1 /* One parameter is expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : pause */
const CLI_Command_Definition_t pauseCommandDefinition =
{
	( const int8_t * ) "pause", /* The command string to type. */
	( const int8_t * ) "pause:\r\n Pause the log with log name (1st par.)\r\n\r\n",
	pauseCommand, /* The function to run. */
	1 /* One parameter is expected. */
};
/*-----------------------------------------------------------*/
/* CLI command structure : resume */
const CLI_Command_Definition_t resumeCommandDefinition =
{
	( const int8_t * ) "resume", /* The command string to type. */
	( const int8_t * ) "resume:\r\n Resume the log with log name (1st par.)\r\n\r\n",
	resumeCommand, /* The function to run. */
	1 /* One parameter is expected. */
};
/*-----------------------------------------------------------*/

/* -----------------------------------------------------------------------
	|												 Private Functions	 														|
   ----------------------------------------------------------------------- 
*/

/* --- H05R0 module initialization. 
*/
void Module_Init(void)
{
	SD_CardInfo CardInfo;
	
	/* Init global variables */
	memset (logs, 0x00U, ((size_t)MAX_LOGS * sizeof(logs)));
	memset (logs, 0x00U, ((size_t)MAX_LOG_VARS * sizeof(logVars)));
	enableSequential = true;
	enableTimeDateHeader = true;
	
	/* Array ports */
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();
	MX_USART3_UART_Init();
	MX_USART4_UART_Init();
	MX_USART5_UART_Init();
	
	/* This module needs more time to process buttons */
	needToDelayButtonStateReset = true;
		
	/* uSD - GPIO and SPI */
	if (BSP_SD_Init() == MSD_ERROR)
	{
		/* No SD card. Insert SD card and reboot */
		while(1) 
		{ 
			IND_ON(); 
			Delay_ms_no_rtos(500); 
			IND_OFF(); 
			Delay_ms_no_rtos(500); 
		};		
	}	
	
	/* Get the uSD size and info */
	BSP_SD_GetCardInfo(&CardInfo);
	
	/* Link the micro SD disk I/O driver */
	if(FATFS_LinkDriver(&SD_Driver, SDPath) == 0)
	{
		/* Mount the default drive */
		if(f_mount(&SDFatFs, SDPath, 1) != FR_OK)
		{
			/* Unmount the drive */
			f_mount(0, SDPath, 0); 
			/* SD card malfunction. Re-insert the card and reboot */
			while(1) 
			{ 
				IND_ON(); 
				Delay_ms_no_rtos(500); 
				IND_OFF(); 
				Delay_ms_no_rtos(500); 
			}	
		}
		else
		{		
			/* Create the logging task */
			xTaskCreate(LogTask, (const char *) "LogTask", (2*configMINIMAL_STACK_SIZE), NULL, osPriorityNormal, &LogTaskHandle);			
		}
	}
	
}
/*-----------------------------------------------------------*/

/* --- H05R0 message processing task. 
*/
Module_Status Module_MessagingTask(uint16_t code, uint8_t port, uint8_t src, uint8_t dst)
{
	Module_Status result = H05R0_OK;
	
	switch (code)
	{

		default:
			result = H05R0_ERR_UnknownMessage;
			break;
	}			

	return result;	
}

/*-----------------------------------------------------------*/

/* --- Get the port for a given UART. 
*/
uint8_t GetPort(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART4)
			return P1;
	else if (huart->Instance == USART2)
			return P2;
	else if (huart->Instance == USART3)
			return P3;
	else if (huart->Instance == USART1)
			return P4;
	else if (huart->Instance == USART5)
			return P5;
	
	return 0;
}

/*-----------------------------------------------------------*/

/* --- Logging task. 
*/
void LogTask(void * argument)
{	
  uint8_t eventResult = 0;
	static uint8_t newline = 1;
  volatile uint8_t i,j,u8lIndexColumn;
  volatile uint8_t u8lFlagToWriteData;
  volatile uint32_t u32lTick = 0;
  volatile uint32_t u32lRate = 0;
	/* Infinite loop */
	for(;;)
	{
		/* Check all active logs */
		for( j=0 ; j<MAX_LOGS ; j++)
		{	
      u32lTick = HAL_GetTick()-logs[j].t0;
      u32lRate = configTICK_RATE_HZ/logs[j].rate;
			if ( (activeLogs >> j) & 0x01 )
			{			
				/* Open this log file if it's closed (and close open one) */
				OpenThisLog(j, &MyFile);
        eventResult = 0;
        if (logs[j].type == EVENT)
        {
          /* check all column to search event in line data of log file */
          for (u8lIndexColumn=0; u8lIndexColumn <MAX_LOG_VARS; u8lIndexColumn++)
          {
            eventResult |= CheckLogVarEvent(u8lIndexColumn);
          }
        }
        u8lFlagToWriteData = 0; /* No writting data into log file */
        newline = 1;            /* Start a new line entry */
				
				/* Check all registered variables for this log */
				for( i=0 ; i<MAX_LOG_VARS ; i++)
				{
					if (logVars[i].type && logVars[i].logIndex == j)
					{
						memset(lineBuffer, 0, sizeof(lineBuffer));
						/* Check for rate or event */
            if ( ((logs[j].type == RATE) && (u32lTick >= u32lRate)) || eventResult )
						{			
							/* Execute this section only once per cycle */
              if (1 == newline)
							{	
								newline = 0;										// Event index written once per line
								
								++(logs[j].sampleCount);				// Advance one sample
								
								/* Write new line */
								f_write(&MyFile, "\n\r", 2, (void *)&byteswritten);	
								
								/* Write index */
								if (logs[j].indexColumnFormat == FMT_TIME)
								{
									GetTimeDate();
									sprintf(lineBuffer, "%02d:%02d:%02d-%03d", BOS.time.hours, BOS.time.minutes, BOS.time.seconds, BOS.time.msec);
								}
								else if (logs[j].indexColumnFormat == FMT_SAMPLE)
								{
									sprintf(lineBuffer, "%d", logs[j].sampleCount);
								}					
							}
						
              /* Write delimiter */
              switch(logs[j].delimiterFormat)
              {
                case FMT_SPACE:
                  strcat(lineBuffer, " ");
                  break;
                case FMT_TAB:
                  strcat(lineBuffer, "\t");
                  break;
                case FMT_COMMA:
                  strcat(lineBuffer, ",");
                  break;
                default:
                  break;
							}
							
							/* Write variable value */
							switch (logVars[i].type)
							{
								case PORT_DIGITAL:
									//sprintf( ( char * ) buffer, "%d", HAL_GPIO_ReadPin());
									//f_write(&MyFile, buffer, 1, (void *)&byteswritten);	
									break;
								
								case PORT_BUTTON:
									switch (button[logVars[i].source].state)
									{
										case OFF:	strcat(lineBuffer, "OFF"); break; 
										case ON:	strcat(lineBuffer, "ON"); break; 
										case OPEN:	strcat(lineBuffer, "OPEN"); break; 
										case CLOSED:	strcat(lineBuffer, "CLOSED"); break; 
										case CLICKED:	strcat(lineBuffer, "CLICKED"); break; 
										case DBL_CLICKED:	strcat(lineBuffer, "DBL_CLICKED"); break; 
										case PRESSED:	strcat(lineBuffer, "PRESSED"); break; 
										case RELEASED:	strcat(lineBuffer, "RELEASED"); break; 
										case PRESSED_FOR_X1_SEC: 
											sprintf((char *)lineBuffer, "%sPRESSED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].pressedX1Sec);
											break;
										case PRESSED_FOR_X2_SEC: 
											sprintf((char *)lineBuffer, "%sPRESSED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].pressedX2Sec);
											break;
										case PRESSED_FOR_X3_SEC: 
											sprintf((char *)lineBuffer, "%sPRESSED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].pressedX3Sec); 
											break;
										case RELEASED_FOR_Y1_SEC:	
											sprintf((char *)lineBuffer, "%sRELEASED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].releasedY1Sec); 
											break;
										case RELEASED_FOR_Y2_SEC:	
											sprintf((char *)lineBuffer, "%sRELEASED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].releasedY2Sec); 
											break;
										case RELEASED_FOR_Y3_SEC:	
											sprintf((char *)lineBuffer, "%sRELEASED_FOR_%d_SEC", (char *) lineBuffer, button[logVars[i].source].releasedY3Sec); 
											break;
										case NONE: 
											if (logs[j].type == RATE) 
											{
												strcat(lineBuffer, "NORMAL");
											}												
											break;								
										default: 
											break;
									}		
									/* Reset button state */
									if (NONE != button[logVars[i].source].state)	
									{
										delayButtonStateReset = false;
									}
									break;
								
								case PORT_DATA:
									
									break;
								
								case MEMORY_DATA_UINT8: 
									sprintf((char *)lineBuffer, "%s%u", (char *)lineBuffer, *(__IO uint8_t *)logVars[i].source);
									break;

								case MEMORY_DATA_INT8: 
									sprintf((char *)lineBuffer, "%s%d", (char *)lineBuffer, *(__IO int8_t *)logVars[i].source); 
									break;

								case MEMORY_DATA_UINT16: 
									sprintf((char *)lineBuffer, "%s%u", (char *)lineBuffer, *(__IO uint16_t *)logVars[i].source); 
									break;

								case MEMORY_DATA_INT16: 
									sprintf((char *)lineBuffer, "%s%d", (char *)lineBuffer, *(__IO int16_t *)logVars[i].source); 
									break;

								case MEMORY_DATA_UINT32: 
									sprintf((char *)lineBuffer, "%s%u", (char *)lineBuffer, *(__IO uint32_t *)logVars[i].source); 
									break;

								case MEMORY_DATA_INT32: 
									sprintf((char *)lineBuffer, "%s%d", (char *)lineBuffer, *(__IO int32_t *)logVars[i].source); 
									break;

								case MEMORY_DATA_FLOAT: 
									sprintf((char *)lineBuffer, "%s%f", (char *)lineBuffer, *(__IO float *)logVars[i].source);
									break;
								
								default:			
									break;
							}							
              u8lFlagToWriteData = 1; /* Setting flag equal "1" to write data into log file */
						}			
						/* Advance samples counter even if event has not occured */
            else if ((u32lTick >= u32lRate) && (!eventResult) && newline)
						{
							newline = 0;										// Execute this only once per line
							++(logs[j].sampleCount);				// Advance one sample
						}			
            else
            {
              /* Nothing to do */
            }

            /* Write the lineBuffer into log file */
            if (1 == u8lFlagToWriteData)
            {
              f_write(&MyFile, lineBuffer, strlen((const char *)lineBuffer), (void *)&byteswritten);
            }
					}					
				}
				f_close(&MyFile);
				/* Start a new line entry */
				/* Reset the rate timer */	
        if (u32lTick >= u32lRate)
        {
					logs[j].t0 = HAL_GetTick();
        }
			}	
			else	
      {
        continue;
		}	
    }
	    taskYIELD(); 
	}								
	
}

/*-----------------------------------------------------------*/

/* --- Check if a logged variable event has occured. 
				varIndex: Log variable array index.
*/
uint8_t CheckLogVarEvent(uint16_t varIndex)
{
	switch (logVars[varIndex].type)
	{
		case PORT_DIGITAL:
			break;
		
		case PORT_BUTTON:
			if (button[logVars[varIndex].source].state != temp_uint8 && button[logVars[varIndex].source].state != 0) {
				temp_uint8 = button[logVars[varIndex].source].state;
				return 1;
			} else if (button[logVars[varIndex].source].state != temp_uint8 && button[logVars[varIndex].source].state == 0) {
				temp_uint8 = button[logVars[varIndex].source].state;
				return 0;
			}
			break;
		
		case PORT_DATA:			
			break;
		
		case MEMORY_DATA_UINT8:
			if (*(__IO uint8_t *)logVars[varIndex].source != (uint8_t)compareValue[varIndex]) {
				*(uint8_t*)&compareValue[varIndex] = *(__IO uint8_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		case MEMORY_DATA_INT8:
			if (*(__IO int8_t *)logVars[varIndex].source != (int8_t)compareValue[varIndex]) {
				*(int8_t*)&compareValue[varIndex] = (int8_t)*(__IO int8_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		case MEMORY_DATA_UINT16:
			if ((uint16_t)*(__IO uint16_t *)logVars[varIndex].source != (uint16_t)compareValue[varIndex]) {
				*(uint16_t*)&compareValue[varIndex] = (uint16_t)*(__IO uint16_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		case MEMORY_DATA_INT16:
			if ((int16_t)*(__IO uint16_t *)logVars[varIndex].source != (int16_t)compareValue[varIndex]) {
				*(int16_t*)&compareValue[varIndex] = (int16_t)*(__IO uint16_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		case MEMORY_DATA_UINT32:
		case MEMORY_DATA_FLOAT:
			if ((uint32_t)*(__IO uint32_t *)logVars[varIndex].source != (uint32_t)compareValue[varIndex]) {
				compareValue[varIndex] = *(__IO uint32_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		case MEMORY_DATA_INT32:
			if ((int32_t)*(__IO uint32_t *)logVars[varIndex].source != (int32_t)compareValue[varIndex]) {
				compareValue[varIndex] = *(__IO uint32_t *)logVars[varIndex].source;
				return 1;
			}
			break;
			
		/*case MEMORY_DATA_FLOAT:
			if (*(__IO uint32_t *)logVars[varIndex].source != (uint32_t)compareValue[varIndex]) {
				compareValue[varIndex] = *(__IO uint32_t *)logVars[varIndex].source;
				return 1;
			}
			break;*/
					
		default:			
			break;
	}
	
	return 0;	
} 

/*-----------------------------------------------------------*/

/* --- Open log file if it's closed (and close open one). 
				logindex: Log array index.
*/
Module_Status OpenThisLog(uint16_t logindex, FIL *objFile)
{
	FRESULT res; 
	char name[15] = {0}; 	
	/* Append log name with extension */
	if ((0U != logs[logindex].file_extension) && (true == enableSequential))
	{
		sprintf((char *)name, "%s_%d%s", logs[logindex].name, logs[logindex].file_extension, ".TXT");
	}
	else
	{
		sprintf((char *)name, "%s%s", logs[logindex].name, ".TXT");
	}
	/* Open this log */			
	res = f_open(objFile, name, FA_OPEN_APPEND | FA_WRITE | FA_READ);
	if (res != FR_OK)	
		return H05R0_ERROR;	
	return H05R0_OK;
} 

/* -----------------------------------------------------------------------
	|																APIs	 																 	|
   ----------------------------------------------------------------------- 
*/

/* --- Create a new data log. 
				logName: Log file name. Max 10 char. 
				type: RATE or EVENT
				rate: data rate in Hz (max 1000 Hz).
				delimiterFormat: FMT_SPACE, FMT_TAB, FMT_COMMA
				indexColumn: FMT_SAMPLE, FMT_TIME
				indexColumnLabel: Index Column label text. Max 30 char.
*/
Module_Status CreateLog(char* logName, logType_t type, float rate, delimiterFormat_t delimiterFormat, indexColumnFormat_t indexColumnFormat,\
	char* indexColumnLabel)
{
	FRESULT res; 
	char name[15] = {0};
	uint8_t i=0;
	uint8_t countFile = 0;
	char *pChar = NULL;
	uint8_t length = 0;
	bool extensionFile = false;
	uint8_t position = 0;
	/* Check if log already exists */
	for( i=0 ; i<MAX_LOGS ; i++)
	{
		if((0U != logs[i].current_extension)  && (true == enableSequential))
		{
			sprintf(name, "%s_%d", logs[i].name, logs[i].current_extension);
		}
		else
		{
			sprintf(name, "%s", logs[i].name);
		}
		if(!strcmp(name, logName))
		{
			return H05R0_ERR_LogNameExists;
		}
	}
	
	/* Check parameters are correct */
	if ( (type != RATE && type != EVENT)	||
			 (delimiterFormat != FMT_SPACE && delimiterFormat != FMT_TAB && delimiterFormat != FMT_COMMA)	||
			 (indexColumnFormat != FMT_NONE && indexColumnFormat != FMT_SAMPLE && indexColumnFormat != FMT_TIME)	||
			 (rate > 1000) )
		return H05R0_ERR_WrongParams;				
					
	/* Name does not exist. Fill first empty location */
	for( i=0 ; i<MAX_LOGS ; i++)
	{
		if(logs[i].name == 0)
		{
			if (true == enableSequential)
			{
				pChar = strchr(logName , '_');
				while (pChar != NULL)
				{
					position = (uint8_t)((uint32_t)pChar - (uint32_t)logName + 1UL);
					pChar = strchr(pChar + 1 , '_');
				}
				/* */
				if(0 != position)
				{
					pChar = logName + position;
			
					length = strlen(pChar);
					while ('\0' != (char)*pChar)
					{
						if((0x30 <= *pChar) && (*pChar <= 0x39))
						{
							countFile += ((uint8_t)(*pChar - 0x30) * numberMap[length - 1]);
						}
						else
						{
							countFile = 0;
							break;
						}
						pChar++;
						length--;
					}
				}
				else
				{
					countFile = 0;
					logs[i].current_extension = 0;
				}
				
				if (countFile != 0)
				{
					extensionFile = true;
					logs[i].current_extension = countFile ;
				}
				else
				{
					position = 0;
					extensionFile = false;
				}
			}
			/* Append log name with extension */
			sprintf((char *)name, "%s%s", logName, ".TXT");
			/* Check if file exists on disk */
			res = f_open(&tempFile, name, FA_CREATE_NEW | FA_WRITE | FA_READ);
			if ((false == enableSequential) && (res == FR_EXIST))
			{
				return H05R0_ERR_LogNameExists;		
			}
			else if ((res != FR_OK) && (FR_EXIST != res))
			{
				return H05R0_ERR_SD;
			}
			else if ((true == enableSequential) && (res == FR_EXIST))
			{
				countFile = 0;
				do
				{
					memset((char *)name, 0, sizeof(name));
					if(false == extensionFile)
					{
						countFile++;
						sprintf(name, "%s_%d%s", logName, countFile, ".TXT");
					}
					else
					{
						strncpy(name, logName, (size_t)((uint32_t)position - 1));
						if(0U == countFile)
						{
							strncat ((char *)name, ".TXT", 4);
						}
						else
						{
							sprintf(name, "%s_%d%s", name, countFile, ".TXT");
						}
						countFile++;
					}
					res = f_open(&tempFile, name, FA_CREATE_NEW | FA_WRITE | FA_READ);
				}while ((FR_EXIST == res) && (MAX_DUPLICATE_FILE > countFile));
				
				if((MAX_DUPLICATE_FILE == countFile) && (FR_EXIST == res))
				{
					return H05R0_ERR_LogNameExists;
				}
				else if (FR_OK != res)
				{
					return H05R0_ERR_SD;
				}
				
				if(true == extensionFile)
				{
					countFile--;
				}
			}
			
			/* Log created successfuly */
			if ((true == enableSequential) && (0U != position))
			{
				logs[i].name = malloc((size_t)position);
				memset(logs[i].name, 0x00U, (size_t)position);
				strncpy(logs[i].name, name, (size_t)(position - 1));
			}
			else
			{
				length = strlen(logName);
				logs[i].name = malloc(length + 1);
				memset(logs[i].name, 0x00U, (size_t)(length + 1));
				strncpy(logs[i].name, logName, (size_t)length);
			}
			logs[i].file_extension = countFile;
			logs[i].type = type;
			logs[i].rate = rate;
			logs[i].delimiterFormat = delimiterFormat;
			logs[i].indexColumnFormat = indexColumnFormat;
			logs[i].indexColumnLabel = indexColumnLabel;
			
			/* Write log header */
			char *buffer = malloc(100);
			memset (buffer, 0x00, 100);
			sprintf(buffer, logHeaderText1, _firmMajor, _firmMinor, _firmPatch, modulePNstring[myPN]);
			res = f_write(&tempFile, buffer, strlen(buffer), (void *)&byteswritten);
			if (enableTimeDateHeader)
			{
				GetTimeDate();
				sprintf(buffer, logHeaderTimeDate, weekdayString[BOS.date.weekday-1], BOS.date.month, BOS.date.day, BOS.date.year, BOS.time.hours, BOS.time.minutes, BOS.time.seconds);
				res = f_write(&tempFile, buffer, strlen(buffer), (void *)&byteswritten);
			}
			if(type == RATE) 
			{
				sprintf(buffer, logHeaderText2, rate);
				res = f_write(&tempFile, buffer, strlen(buffer), (void *)&byteswritten);
			} 
			else if (type == EVENT) 
			{
				res = f_write(&tempFile, logHeaderText3, strlen(logHeaderText3), (void *)&byteswritten);	
			}
			
			/* Write index label */
			res = f_write(&tempFile, indexColumnLabel, strlen(indexColumnLabel), (void *)&byteswritten);
			
			f_close(&tempFile);
			free(buffer);
			
			return H05R0_OK;
		}
  }	

	return H05R0_ERR_MaxLogs;	
}

/*-----------------------------------------------------------*/

/* --- Save data from a source to an existing data log. 
				logName: Log file name.
				type: PORT_DIGITAL, PORT_DATA, PORT_BUTTON, MEMORY_DATA.
				source: data source. Ports (P1-Px), buttons (B1-Bx) or memory location.
				columnLabel: Column label text. Max 30 char.
*/
Module_Status LogVar(char* logName, logVarType_t type, uint32_t source, char* ColumnLabel)
{
	uint8_t i = 0, j = 0;
	char tempName[15] = {0};
	/* Search for this log to make sure it exists */
	for( j=0 ; j<MAX_LOGS ; j++)
	{
		if((0 != logs[j].current_extension) && (true == enableSequential))
		{
			sprintf(tempName, "%s_%d", logs[j].name, logs[j].current_extension);
		}
		else
		{
			sprintf(tempName, "%s", logs[j].name);
		}
		
		if (!strcmp(tempName, logName))
		{
			/* Make sure there's enough space for this log variable */
			for( i=0 ; i<MAX_LOG_VARS ; i++)
			{
				if(logVars[i].type == 0)
				{		
					logVars[i].type = type;
					logVars[i].source = source;
					logVars[i].logIndex = j;
					logVars[i].varLabel = ColumnLabel;
					
					/* Write delimiter */
					OpenThisLog(j, &tempFile);
					if (logs[j].delimiterFormat == FMT_SPACE)
						f_write(&tempFile, " ", 1, (void *)&byteswritten);
					else if (logs[j].delimiterFormat == FMT_TAB)
						f_write(&tempFile, "\t", 1, (void *)&byteswritten);
					else if (logs[j].delimiterFormat == FMT_COMMA)
						f_write(&tempFile, ",", 1, (void *)&byteswritten);
					/* Write variable label */
					f_write(&tempFile, ColumnLabel, strlen(ColumnLabel), (void *)&byteswritten);
					
					f_close(&tempFile);
					
					return H05R0_OK;
				}
			}
			return H05R0_ERR_MaxLogVars;
		}
	}		

	return H05R0_ERR_LogDoesNotExist;	
}

/*-----------------------------------------------------------*/

/* --- Start an existing data log. 
				logName: Log file name.
*/
Module_Status StartLog(char* logName)
{
	uint8_t j = 0;
	char tempName[15] = {0};
	/* Search for this log to make sure it exists */
	for( j=0 ; j<MAX_LOGS ; j++)
	{
		if((0U != logs[j].current_extension) && (true == enableSequential))
		{
			sprintf(tempName, "%s_%d", logs[j].name, logs[j].current_extension);
		}
		else
		{
			sprintf(tempName, "%s", logs[j].name);
		}
		if (!strcmp(tempName, logName))
		{
			activeLogs |= (0x01 << j);
			logs[j].sampleCount = 0;
			logs[j].t0 = HAL_GetTick();
			OpenThisLog(j, &tempFile);
			/* Write new line */
			f_write(&tempFile, "\n\r", 2, (void *)&byteswritten);
			
			f_close(&tempFile);
			
			return H05R0_OK;
		}		
	}

	return H05R0_ERR_LogDoesNotExist;	
}

/*-----------------------------------------------------------*/

/* --- Stop a running data log. 
				logName: Log file name.
*/
Module_Status StopLog(char* logName)
{
	volatile uint8_t j = 0;
	char tempName[15] = {0};
	/* Search for this log to make sure it exists */
	for( j=0 ; j<MAX_LOGS ; j++)
	{
		if((0U != logs[j].current_extension) && (true == enableSequential))
		{
			sprintf(tempName, "%s_%d", logs[j].name, logs[j].current_extension);
		}
		else
		{
			sprintf(tempName, "%s", logs[j].name);
		}
		if (!strcmp(tempName, logName))
		{
			if ( (activeLogs >> j) & 0x01 )
			{
				/* StopLog only inactive log, don't reset variable*/
				activeLogs &= ~(0x01 << j);
				return H05R0_OK;
			}
			else
			{
				return H05R0_ERR_LogIsNotActive;
			}
		}		
	}
	return H05R0_ERR_LogDoesNotExist;	
}

/*-----------------------------------------------------------*/

/* --- Pause a running data log. 
				logName: Log file name.
*/
Module_Status PauseLog(char* logName)
{
	uint8_t j = 0;
	char tempName[15] = {0};
	/* Search for this log to make sure it exists */
	for( j=0 ; j<MAX_LOGS ; j++)
	{
		if((0U != logs[j].current_extension)  && (true == enableSequential))
		{
			sprintf(tempName, "%s_%d", logs[j].name, logs[j].current_extension);
		}
		else
		{
			sprintf(tempName, "%s", logs[j].name);
		}
		if (!strcmp(tempName, logName))
		{
			if ( (activeLogs >> j) & 0x01 )
			{
				activeLogs &= ~(0x01 << j);
				return H05R0_OK;
			}
			else
				return H05R0_ERR_LogIsNotActive;
		}		
	}

	return H05R0_ERR_LogDoesNotExist;	
} 

/*-----------------------------------------------------------*/

/* --- Resume a paused data log. 
				logName: Log file name.
*/
Module_Status ResumeLog(char* logName)
{
	uint8_t j = 0;
	char tempName[15] = {0};
	/* Search for this log to make sure it exists */
	for( j=0 ; j<MAX_LOGS ; j++)
	{
		if((0U != logs[j].current_extension)  && (true == enableSequential))
		{
			sprintf(tempName, "%s_%d", logs[j].name, logs[j].current_extension);
		}
		else
		{
			sprintf(tempName, "%s", logs[j].name);
		}
		if (!strcmp(tempName, logName))
		{
			activeLogs |= (0x01 << j);
			return H05R0_OK;
		}		
	}

	return H05R0_ERR_LogDoesNotExist;	
} 

/*-----------------------------------------------------------*/

/* --- Delete an existing data log. 
				logName: Log file name.
				options: DELETE_ALL, KEEP_ON_DISK
*/
Module_Status DeleteLog(char* logName, options_t options)
{
	Module_Status result = H05R0_OK;

	
	// Free ptemp vars
	
	return result;	
}

/*-----------------------------------------------------------*/
/* -----------------------------------------------------------------------
	|															Commands																 	|
   ----------------------------------------------------------------------- 
*/

portBASE_TYPE addLogCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1, *pcParameterString2, *pcParameterString3, *pcParameterString4, *pcParameterString5, *pcParameterString6; 
	portBASE_TYPE xParameterStringLength1 = 0, xParameterStringLength2 = 0, xParameterStringLength3 = 0; 
	portBASE_TYPE xParameterStringLength4 = 0, xParameterStringLength5 = 0, xParameterStringLength6 = 0;
	logType_t type; delimiterFormat_t dformat; indexColumnFormat_t iformat; float rate;
	char *name, *index;
	static const int8_t *pcOKMessage = ( int8_t * ) "Log created successfully\r\n";
	static const int8_t *pcWrongValue = ( int8_t * ) "Log creation failed. Wrong parameters\r\n";
	static const int8_t *pcLogExists = ( int8_t * ) "Log creation failed. Log name already exists\r\n";
	static const int8_t *pcSDerror = ( int8_t * ) "Log creation failed. SD card error\r\n";
	static const int8_t *pcMaxLogs = ( int8_t * ) "Log creation failed. Maximum number of logs reached\r\n";
	static const int8_t *pcMemoryFull = ( int8_t * ) "Variable was not added to log. Internal memory full\r\n";
	
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);
	/* Obtain the 2nd parameter string: log type */
	pcParameterString2 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 2, &xParameterStringLength2);
	/* Obtain the 3rd parameter string: log rate */
	pcParameterString3 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 3, &xParameterStringLength3);
	/* Obtain the 4th parameter string: delimiter format */
	pcParameterString4 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 4, &xParameterStringLength4);
	/* Obtain the 5th parameter string: index format */
	pcParameterString5 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 5, &xParameterStringLength5);
	/* Obtain the 6th parameter string: index label */
	pcParameterString6 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 6, &xParameterStringLength6);
	
	/* log name */
	pcParameterString1[xParameterStringLength1] = 0;		// Get rid of the remaining parameters
	name = (char *)malloc(strlen((const char *)pcParameterString1) + 1);		// Move string out of the stack
	memset (name, 0, strlen((const char *)pcParameterString1) + 1);
	if (name == NULL)	
		result = H05R0_ERR_MemoryFull;
	else
		strcpy(name, (const char *)pcParameterString1);
	
	/* type */
	if (!strncmp((const char *)pcParameterString2, "rate", xParameterStringLength2))
		type = RATE;
	else if (!strncmp((const char *)pcParameterString2, "event", xParameterStringLength2))
		type = EVENT;		
	else
		result = H05R0_ERR_WrongParams;
	
	/* rate */
	rate = atof( ( const char * ) pcParameterString3 );
	if (rate < 0.0f || rate > 1000.0f)
		result = H05R0_ERR_WrongParams;
	
	/* delimiter format */
	if (!strncmp((const char *)pcParameterString4, "space", xParameterStringLength4))
		dformat = FMT_SPACE;
	else if (!strncmp((const char *)pcParameterString4, "tab", xParameterStringLength4))
		dformat = FMT_TAB;		
	else if (!strncmp((const char *)pcParameterString4, "comma", xParameterStringLength4))
		dformat = FMT_COMMA;		
	else
		result = H05R0_ERR_WrongParams;
	
	/* index format */
	if (!strncmp((const char *)pcParameterString5, "sample", xParameterStringLength5))
		iformat = FMT_SAMPLE;
	else if (!strncmp((const char *)pcParameterString5, "time", xParameterStringLength5))
		iformat = FMT_TIME;		
	else if (!strncmp((const char *)pcParameterString5, "none", xParameterStringLength5))
		iformat = FMT_NONE;		
	else
		result = H05R0_ERR_WrongParams;

	/* index name */
	pcParameterString6[xParameterStringLength6] = 0;		// Get rid of the remaining parameters
	index = (char *)malloc(strlen((const char *)pcParameterString6) + 1);		// Move string out of the stack
	memset (index, 0, strlen((const char *)pcParameterString6) + 1);
	if (index == NULL)	
		result = H05R0_ERR_MemoryFull;
	else
		strcpy(index, (const char *)pcParameterString6);
	
	/* Create the log */
	if (result == H05R0_OK) {
		result = CreateLog(name, type, rate, dformat, iformat, index);	
	} else {
		free(index);
	}
	free(name);
	/* Respond to the command */
	if (result == H05R0_OK) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage);
	} else if (result == H05R0_ERR_WrongParams) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcWrongValue);
	} else if (result ==  H05R0_ERR_LogNameExists) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogExists);
	} else if (result ==  H05R0_ERR_SD) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcSDerror);
	} else if (result ==  H05R0_ERR_MaxLogs) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcMaxLogs);
	} else if (result ==  H05R0_ERR_MemoryFull) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcMemoryFull);
	}
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

portBASE_TYPE deleteLogCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1, *pcParameterString2; 
	portBASE_TYPE xParameterStringLength1 = 0, xParameterStringLength2 = 0; 
	options_t options; 
	static const int8_t *pcOKMessage1 = ( int8_t * ) "Log deleted both internally and from the disk\r\n";
	static const int8_t *pcOKMessage2 = ( int8_t * ) "Log deleted internally and kept on the disk\r\n";
	static const int8_t *pcWrongValue = ( int8_t * ) "Log deletion failed. Wrong parameters\r\n";
	
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);
	/* Obtain the 2nd parameter string: delete options */
	pcParameterString2 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 2, &xParameterStringLength2);
	
	/* log name */
	pcParameterString1[xParameterStringLength1] = 0;		// Get rid of the remaining parameters
	
	/* type */
	if (!strncmp((const char *)pcParameterString2, "all", xParameterStringLength2))
		options = DELETE_ALL;
	else if (!strncmp((const char *)pcParameterString2, "keepdisk", xParameterStringLength2))
		options = KEEP_ON_DISK;		
	else
		result = H05R0_ERR_WrongParams;

	/* Delete the log */
	if (result == H05R0_OK) {
		result = DeleteLog((char *)pcParameterString1, options);	
	}
	
	/* Respond to the command */
	if (result == H05R0_OK && options == DELETE_ALL) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage1);
	} else if (result == H05R0_OK && options == KEEP_ON_DISK) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage2);
	} else if (result == H05R0_ERR_WrongParams) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcWrongValue);
	} 
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

portBASE_TYPE logVarCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1, *pcParameterString2, *pcParameterString3, *pcParameterString4, *pcParameterString5; 
	portBASE_TYPE xParameterStringLength1 = 0, xParameterStringLength2 = 0, xParameterStringLength3 = 0; 
	portBASE_TYPE xParameterStringLength4 = 0, xParameterStringLength5 = 0;
	logVarType_t type; uint32_t source; char *label;
	static const int8_t *pcOKMessage = ( int8_t * ) "Variable added to log successfully\r\n";
	static const int8_t *pcWrongValue = ( int8_t * ) "Variable was not added to log. Wrong parameters\r\n";
	static const int8_t *pcLogDoesNotExist = ( int8_t * ) "Variable was not added to log. Log does not exist\r\n";
	static const int8_t *pcMemoryFull = ( int8_t * ) "Variable was not added to log. Internal memory full\r\n";
	static const int8_t *pcMaxLogVars = ( int8_t * ) "Variable was not added to log. Maximum number of logged variables reached\r\n";
	
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);
	/* Obtain the 2nd parameter string: variable type 1 */
	pcParameterString2 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 2, &xParameterStringLength2);
	/* Obtain the 3rd parameter string: variable type 2 */
	pcParameterString3 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 3, &xParameterStringLength3);
	/* Obtain the 4th parameter string: variable source */
	pcParameterString4 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 4, &xParameterStringLength4);
	/* Obtain the 5th parameter string: variable column label */
	pcParameterString5 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 5, &xParameterStringLength5);
	
	/* log name */
	pcParameterString1[xParameterStringLength1] = 0;		// Get rid of the remaining parameters
	
	/* variable type */
	if (!strncmp((const char *)pcParameterString2, "port", xParameterStringLength2)) {
		if (!strncmp((const char *)pcParameterString3, "digital", xParameterStringLength3)) {
			type = PORT_DIGITAL;
		} else if (!strncmp((const char *)pcParameterString3, "data", xParameterStringLength3)) {
			type = PORT_DATA;
		} else if (!strncmp((const char *)pcParameterString3, "button", xParameterStringLength3)) {
			type = PORT_BUTTON;
		} else
			result = H05R0_ERR_WrongParams;
	} else if (!strncmp((const char *)pcParameterString2, "memory", xParameterStringLength2)) {
		if (!strncmp((const char *)pcParameterString3, "uint8", xParameterStringLength3)) {
			type = MEMORY_DATA_UINT8;
		} else if (!strncmp((const char *)pcParameterString3, "int8", xParameterStringLength3)) {
			type = MEMORY_DATA_INT8;
		} else if (!strncmp((const char *)pcParameterString3, "uint16", xParameterStringLength3)) {
			type = MEMORY_DATA_UINT16;
		} else if (!strncmp((const char *)pcParameterString3, "int16", xParameterStringLength3)) {
			type = MEMORY_DATA_INT16;
		} else if (!strncmp((const char *)pcParameterString3, "uint32", xParameterStringLength3)) {
			type = MEMORY_DATA_UINT32;
		} else if (!strncmp((const char *)pcParameterString3, "int32", xParameterStringLength3)) {
			type = MEMORY_DATA_INT32;
		} else if (!strncmp((const char *)pcParameterString3, "float", xParameterStringLength3)) {
			type = MEMORY_DATA_FLOAT;
		} else
			result = H05R0_ERR_WrongParams;		
	} else
		result = H05R0_ERR_WrongParams;
	
	/* source */
	if (type == PORT_BUTTON && pcParameterString4[0] == 'b')
		source = ( uint8_t ) atol( ( char * ) pcParameterString4+1 );
	else if ((type == PORT_DIGITAL || type == PORT_DATA) && pcParameterString4[0] == 'p')
		source = ( uint8_t ) atol( ( char * ) pcParameterString4+1 );
	else if (!strncmp((const char *)pcParameterString4, "0x", 2)) {
		source = strtoul(( const char * ) pcParameterString4, NULL, 16);
		if ((source < FLASH_BASE || source > (FLASH_BASE+FLASH_SIZE)) && (source < SRAM_BASE || source > (SRAM_BASE+SRAM_SIZE)))
			result = H05R0_ERR_WrongParams;
	} else
		result = H05R0_ERR_WrongParams;
	
	/* variable column label */
	pcParameterString5[xParameterStringLength5] = 0;		// Get rid of the remaining parameters
	label = (char *)malloc(strlen((const char *)pcParameterString5) + 1);		// Move string out of the stack
	memset (label, 0, strlen((const char *)pcParameterString5) + 1);
	if (label == NULL)	
		result = H05R0_ERR_MemoryFull;
	else
		strcpy(label, (const char *)pcParameterString5);
	
	/* Add the variable to the log */
	if (result == H05R0_OK) {
		result = LogVar((char *)pcParameterString1, type, source, label);	
	} else {
		free(label);
	}
	
	/* Respond to the command */
	if (result == H05R0_OK) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage);
	} else if (result == H05R0_ERR_WrongParams) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcWrongValue);
	} else if (result ==  H05R0_ERR_LogDoesNotExist) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogDoesNotExist);
	} else if (result ==  H05R0_ERR_MemoryFull) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcMemoryFull);
	} else if (result ==  H05R0_ERR_MaxLogVars) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcMaxLogVars);
	}
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

portBASE_TYPE startCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1; 
	portBASE_TYPE xParameterStringLength1 = 0;  
	static const int8_t *pcOKMessage = ( int8_t * ) "%s started logging\r\n";
	static const int8_t *pcLogDoesNotExist = ( int8_t * ) "Log does not exist\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);

	/* Start the log */
	if (result == H05R0_OK) {
		result = StartLog((char *)pcParameterString1);	
	}
	
	/* Respond to the command */
	if (result == H05R0_OK) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogDoesNotExist) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogDoesNotExist);
	} 
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/
portBASE_TYPE stopCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1; 
	portBASE_TYPE xParameterStringLength1 = 0;  
	static const int8_t *pcOKMessage = ( int8_t * ) "%s stoped logging\r\n";
	static const int8_t *pcIsNotActive = ( int8_t * ) "%s was not running\r\n";
	static const int8_t *pcLogDoesNotExist = ( int8_t * ) "Log does not exist\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);

	/* Stop the log */
	if (result == H05R0_OK) {
		result = StopLog((char *)pcParameterString1);	
	}
	
	/* Respond to the command */
	if (result == H05R0_OK) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogIsNotActive) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcIsNotActive, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogDoesNotExist) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogDoesNotExist);
	} 
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

portBASE_TYPE pauseCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1; 
	portBASE_TYPE xParameterStringLength1 = 0; 
	static const int8_t *pcOKMessage = ( int8_t * ) "%s paused logging\r\n";
	static const int8_t *pcIsNotActive = ( int8_t * ) "%s was not running\r\n";
	static const int8_t *pcLogDoesNotExist = ( int8_t * ) "Log does not exist\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);

	/* Pause the log */
	if (result == H05R0_OK) {
		result = PauseLog((char *)pcParameterString1);	
	}
	
	/* Respond to the command */
	if (result == H05R0_OK) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogIsNotActive) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcIsNotActive, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogDoesNotExist) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogDoesNotExist);
	}  
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

portBASE_TYPE resumeCommand( int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString )
{
	Module_Status result = H05R0_OK;
	
	int8_t *pcParameterString1; 
	portBASE_TYPE xParameterStringLength1 = 0; 
	static const int8_t *pcOKMessage = ( int8_t * ) "%s resumed logging\r\n";
	static const int8_t *pcLogDoesNotExist = ( int8_t * ) "Log does not exist\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	
	/* Obtain the 1st parameter string: log name */
	pcParameterString1 = ( int8_t * ) FreeRTOS_CLIGetParameter (pcCommandString, 1, &xParameterStringLength1);

	/* Resume the log */
	if (result == H05R0_OK) {
		result = ResumeLog((char *)pcParameterString1);	
	}
	
	/* Respond to the command */
	if (result == H05R0_OK) {
		sprintf( ( char * ) pcWriteBuffer, ( char * ) pcOKMessage, pcParameterString1);
	} else if (result ==  H05R0_ERR_LogDoesNotExist) {
		strcpy( ( char * ) pcWriteBuffer, ( char * ) pcLogDoesNotExist);
	} 
	
	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}

/*-----------------------------------------------------------*/

/************************ (C) COPYRIGHT HEXABITZ *****END OF FILE****/
