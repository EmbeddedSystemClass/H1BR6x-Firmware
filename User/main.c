/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
	
/*
		MODIFIED by Hexabitz for BitzOS (BOS) V0.1.3 - Copyright (C) 2017 Hexabitz
    All rights reserved
*/

/* Includes ------------------------------------------------------------------*/
#include "BOS.h"


/* Private variables ---------------------------------------------------------*/


/* Private function prototypes -----------------------------------------------*/



/* Main functions ------------------------------------------------------------*/

int main(void)
{


  /* MCU Configuration----------------------------------------------------------*/

  /* Reset all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all user peripherals */

	/* Initialize BitzOS */
	BOS_Init();

  /* Call init function for freertos objects (in freertos.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();
  
  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  while (1)
  {


  }


}

/*-----------------------------------------------------------*/

/* FrontEndTask function */
void FrontEndTask(void * argument)
{
	AddPortButton(MOMENTARY_NO, P1);								// Magnetic
	SetButtonEvents(P1, 0, 0, 1, 0, 0, 1, 0, 0);
	AddPortButton(MOMENTARY_NO, P3);								// Limit Switch 1
	SetButtonEvents(P3, 1, 0, 3, 0, 0, 0, 0, 0);
	AddPortButton(MOMENTARY_NO, P4);								// Limit Switch 2
	SetButtonEvents(P4, 1, 0, 3, 0, 0, 0, 0, 0);

	/* Create log and log switches */
	CreateLog("Event", EVENT, 10, FMT_COMMA, FMT_SAMPLE, "#");
	LogVar("Event", PORT_BUTTON, P1, "Magnetic");
	LogVar("Event", PORT_BUTTON, P3, "LS1");
	LogVar("Event", PORT_BUTTON, P4, "LS2");

	CreateLog("Rate", RATE, 10, FMT_COMMA, FMT_SAMPLE, "#");
	LogVar("Rate", PORT_BUTTON, P1, "Magnetic");
	LogVar("Rate", PORT_BUTTON, P3, "LS1");
	LogVar("Rate", PORT_BUTTON, P4, "LS2");
	
	IND_ON(); 
	
	StartLog("Event"); StartLog("Rate");
	Delay_s(20);	
	StopLog("Event"); StopLog("Rate");
	
	IND_OFF();				// Signal the end of the log
	Delay_ms(300); IND_blink(200);		
	Delay_ms(300); IND_blink(200);
	
  /* Infinite loop */
  for(;;)
  {

	}
}

/*-----------------------------------------------------------*/

/************************ (C) COPYRIGHT HEXABITZ *****END OF FILE****/
