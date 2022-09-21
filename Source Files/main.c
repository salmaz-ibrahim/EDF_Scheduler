/*
 * FreeRTOS Kernel V10.2.0
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* 
	NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	The processor MUST be in supervisor mode when vTaskStartScheduler is 
	called.  The demo applications included in the FreeRTOS.org download switch
	to supervisor mode prior to main being called.  If you are not using one of
	these demo application projects then ensure Supervisor mode is used.
*/


/*
 * Creates all the demo application tasks, then starts the scheduler.  The WEB
 * documentation provides more details of the demo application tasks.
 * 
 * Main.c also creates a task called "Check".  This only executes every three 
 * seconds but has the highest priority so is guaranteed to get processor time.  
 * Its main function is to check that all the other tasks are still operational.
 * Each task (other than the "flash" tasks) maintains a unique count that is 
 * incremented each time the task successfully completes its function.  Should 
 * any error occur within such a task the count is permanently halted.  The 
 * check task inspects the count of each task to ensure it has changed since
 * the last time the check task executed.  If all the count variables have 
 * changed all the tasks are still executing error free, and the check task
 * toggles the onboard LED.  Should any task contain an error at any time 
 * the LED toggle rate will change from 3 seconds to 500ms.
 *
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "lpc21xx.h"
#include "queue.h"

/* Peripheral includes. */
#include "serial.h"
#include "GPIO.h"

/*-----------------------------------------------------------*/

/* Constants to setup I/O and processor. */
#define mainBUS_CLK_FULL	( ( unsigned char ) 0x01 )

/* Constants for the ComTest demo application tasks. */
#define mainCOM_TEST_BAUD_RATE	( ( unsigned long ) 9600 ) //ser9600

/* Task Handlers */
TaskHandle_t Button_1_Monitor_Handle = NULL;
TaskHandle_t Button_2_Monitor_Handle = NULL;
TaskHandle_t Periodic_Transmitter_Handle = NULL;
TaskHandle_t Uart_Receiver_Handle = NULL;
TaskHandle_t Load_1_Simulation_Handle = NULL;
TaskHandle_t Load_2_Simulation_Handle = NULL;


uint32  task_1_in_time =0 , task_1_out_time=0 , task_1_total_time;
uint32  task_2_in_time=0, task_2_out_time=0,task_2_total_time;
uint32  cpu_load;
uint32  system_time;

/*Buffer for Run-Time Stats*/
char SystemStats[190];

/* Queue Handlers */
QueueHandle_t MessageQueue1 = NULL;
QueueHandle_t MessageQueue2 = NULL;
QueueHandle_t MessageQueue3 = NULL;

/*
 * Configure the processor for use with the Keil demo board.  This is very
 * minimal as most of the setup is managed by the settings in the project
 * file.
 */
static void prvSetupHardware( void );
/*-----------------------------------------------------------*/
void Button_1_Monitor_Task(void* pvParameters)
{
	pinState_t Button1_CurrentState;
	pinState_t Button1_PreviousState = GPIO_read(PORT_1, PIN0);
	TickType_t xLastWakeTime = xTaskGetTickCount();
	signed Edge_Flag = 0;


	for(;;)
	{
		/* Read current state for button 1 */
		Button1_CurrentState = GPIO_read(PORT_1, PIN0);
		
		/* Detect the edge */
		if ( ( Button1_CurrentState == PIN_IS_HIGH ) && ( Button1_PreviousState == PIN_IS_LOW ) )
		{
			/*Rising edge */
			Edge_Flag = '+';
		}
		else if ( ( Button1_CurrentState == PIN_IS_LOW ) && ( Button1_PreviousState == PIN_IS_HIGH ) )
		{
			/*Falling edge */
			Edge_Flag = '-';
		}
		else
		{
			/*No event*/
			Edge_Flag = '=';
		}
		
		/* Update the previous (Reference) State */
		Button1_PreviousState = Button1_CurrentState;
		
		/* Send the new news to the consumer */
		xQueueOverwrite(MessageQueue1, &Edge_Flag );
		
		/* priodicity = 50 ms */
		vTaskDelayUntil(&xLastWakeTime, 50);
	}
}

void Button_2_Monitor_Task(void* pvParameters)
{
	pinState_t Button2_CurrentState;
	pinState_t Button2_PreviousState = GPIO_read(PORT_1, PIN1);
	TickType_t xLastWakeTime = xTaskGetTickCount();
	signed Edge_Flag = 0;

	/* Warning: I ignored debaunce effect as we run on simulation not real hardware */
	for(;;)
	{
		/* Read current state for button 1 */
		Button2_CurrentState = GPIO_read(PORT_1, PIN1);
		
		/* Detect the edge */
		if ( ( Button2_CurrentState == PIN_IS_HIGH ) && ( Button2_PreviousState == PIN_IS_LOW ) )
		{
			/* Rising edge  */
			Edge_Flag = '+';
		}
		else if ( ( Button2_CurrentState == PIN_IS_LOW ) && ( Button2_PreviousState == PIN_IS_HIGH ) )
		{
			/*Falling edge  */
			Edge_Flag = '-';
		}
		else
		{
			/* No edge is detected */
			Edge_Flag = '=';
		}
		
		/* Update the previous (Reference) State */
		Button2_PreviousState = Button2_CurrentState;
		
		/* Send the new news to the consumer */
		xQueueOverwrite(MessageQueue2, &Edge_Flag );
		
		/* priodicity = 50 ms */
		vTaskDelayUntil(&xLastWakeTime, 50);
	}
}

void Periodic_Transmitter_Task(void* pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint8 index = 0;
	char String[6];
	strcpy(String, "\nWaiting");
	String[5] = '\0';
	for(;;)
	{
		/* Send the string to Uart_Receiver_Task character by character */
		for (index = 0; index < 5; index++)
		{
			xQueueSend(MessageQueue3, String + index, 100);
		}
		
		/* priodicity = 100 ms */
		vTaskDelayUntil(&xLastWakeTime, 100);
	}
}

void Uart_Receiver_Task(void* pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	signed char Button1_Data;
	signed char Button2_Data;
	char StringReceived[6];
	uint8 index = 0;

	for(;;)
	{
		index = 0;
		/* Button 1  */
		if ( xQueueReceive(MessageQueue1, &Button1_Data, 0) && ( Button1_Data != '=' ) )
		{

			xSerialPutChar('\n');		
			xSerialPutChar('B');
			xSerialPutChar('1');
			xSerialPutChar(':');
			xSerialPutChar(' ');
			xSerialPutChar(Button1_Data);
			xSerialPutChar('v');
			xSerialPutChar('e');
		}
		else
		{	
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
		}
		
		index = 0;
		/* Button 2  */
		if ( xQueueReceive(MessageQueue2, &Button2_Data, 0) && ( Button2_Data != '=' ) )
		{
			xSerialPutChar('\n');		
			xSerialPutChar('B');
			xSerialPutChar('2');
			xSerialPutChar(':');
			xSerialPutChar(' ');
			xSerialPutChar(Button2_Data);
			xSerialPutChar('v');
			xSerialPutChar('e');
		}
		else
		{
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
			xSerialPutChar(' ');
		}
		
		index = 0;
		/* Periodic_Transmitter_Task String */
		if ( uxQueueMessagesWaiting(MessageQueue3) != 0)
		{
			for(index = 0; index < 5; index++)
			{
				xQueueReceive(MessageQueue3, StringReceived + index, 0);
			}

			for(index = 0; index < 5; index++)
			{
				/* Send the name of button character by character */
				xSerialPutChar(StringReceived[index]);
			}
			
			xQueueReset(MessageQueue3);
		}
			#if(GetRunTimeStats ==1)
					vTaskGetRunTimeStats(SystemStats);
					vSerialPutString((uint8*)SystemStats, strlen(SystemStats));
					xSerialPutChar('\n');
					xSerialPutChar('\n');			
			#endif
			/*Periodicity = 20 ms*/
		vTaskDelayUntil(&xLastWakeTime, 20);
	}
}

void Load_1_Simulation_Task(void* pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint32 counter = 0;

	for(;;)
	{
		/* Execute for 5 ms */
		Wait_ms(5);
		
		/* priodicity = 10 ms */
		vTaskDelayUntil(&xLastWakeTime, 10);
	}
}

void Load_2_Simulation_Task(void* pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint32 counter = 0;

	for(;;)
	{
		/* Execute for 12 ms */
		Wait_ms(12);
		
		/* priodicity = 100 ms */
		vTaskDelayUntil(&xLastWakeTime, 100);
	}
}

/*
 * Application entry point:
 * Starts all the other tasks, then starts the scheduler. 
 */
int main( void )

{
	/* Setup the hardware for use with the Keil demo board. */
	prvSetupHardware();
	
	/* Message Queues Creation */
	MessageQueue1 = xQueueCreate(1, sizeof(char));
	MessageQueue2 = xQueueCreate(1, sizeof(char));
	MessageQueue3 = xQueueCreate(5, sizeof(char));

	/* Create Tasks here */
	xTaskPeriodicCreate(Button_1_Monitor_Task,
						"Button 1 Monitor Task",
						100, (void*)0,
						1,
						&Button_1_Monitor_Handle,
						50);
						
	xTaskPeriodicCreate(Button_2_Monitor_Task,
						"Button 2 Monitor Task",
						100,
						(void*)0,
						1,
						&Button_2_Monitor_Handle,
						50);
						
	xTaskPeriodicCreate(Periodic_Transmitter_Task,
						"Periodic Transmitter Task",
						100,
						(void*)0,
						1,
						&Periodic_Transmitter_Handle,
						100);
						
	xTaskPeriodicCreate(Uart_Receiver_Task,
						"Uart Receiver Task",
						100,
						(void*)0,
						1,
						&Uart_Receiver_Handle,
						20);
						
	xTaskPeriodicCreate(Load_1_Simulation_Task,
						"Load 1 Simulation Task",
						100,
						(void*)0,
						1,
						&Load_1_Simulation_Handle,
						10);
						
	xTaskPeriodicCreate(Load_2_Simulation_Task,
						"Load 2 Simulation Task",
						100,
						(void*)0,
						1,
						&Load_2_Simulation_Handle,
						100);
		
	/* Now all the tasks have been started - start the scheduler.

	NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	The processor MUST be in supervisor mode when vTaskStartScheduler is 
	called.  The demo applications included in the FreeRTOS.org download switch
	to supervisor mode prior to main being called.  If you are not using one of
	these demo application projects then ensure Supervisor mode is used here. */
	vTaskStartScheduler();

	/* Should never reach here!  If you do then there was not enough heap
	available for the idle task to be created. */
	for( ;; );
}
/*-----------------------------------------------------------*/
/* Implement Tick Hook */
void vApplicationTickHook(void)
{
	/* Write your code here! */
	GPIO_write(PORT_0, PIN0, PIN_IS_HIGH);
	GPIO_write(PORT_0, PIN0, PIN_IS_LOW);
}

/* Function to reset timer 1 */
void timer1Reset(void)
{
	T1TCR |= 0x2;
	T1TCR &= ~0x2;
}

/* Function to initialize and start timer 1 */
static void configTimer1(void)
{
	T1PR = 1000;
	T1TCR |= 0x1;
}

static void prvSetupHardware( void )
{
	/* Perform the hardware setup required.  This is minimal as most of the
	setup is managed by the settings in the project file. */

	/* Configure UART */
	xSerialPortInitMinimal(mainCOM_TEST_BAUD_RATE);

	/* Configure GPIO */
	GPIO_init();
	
	/* Config trace timer 1 and read T1TC to get current tick */
	configTimer1();

	/* Setup the peripheral bus to be the same as the PLL output. */
	VPBDIV = mainBUS_CLK_FULL;
}
/*-----------------------------------------------------------*/