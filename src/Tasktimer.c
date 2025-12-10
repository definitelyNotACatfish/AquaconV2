// Includes
#include "main.h"
#include "Tasktimer.h"
#include "time.h"
#include "DS1307.h"
#include <stm32f4xx.h>
#include <string.h>
#ifdef Tasktimer_Debug
	#include "USART.h"
#endif

/*

************************************************************************************************
* Tasktimer mit TIM2 auf dem STM32F401RET6												   	   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Variablen
struct Task gl_Tasktimer_Tasks[Tasktimer_MAX_Tasks];				// Speicherplatz fuer Tasks reservieren
void (*gl_Tasktimer_Functionspointer[Tasktimer_MAX_Tasks])(void);	// Funktion die Ausgefuehrt werden soll
volatile uint32_t gl_Tasktimer_Timestamp=0;							// Counter f�r die ISR
volatile uint32_t gl_Tasktimer_Uptime=0;							// Uptime des Servers

// Funktionen
void Tasktimer_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;								// Clock f�r TIM2 an

	TIM2->CR1 = TIM_CR1_DIR;										// Downcounter
	TIM2->PSC = (F_CPU/65536)-1;									// Teiler = PSC + 1		(Eigendlich 1556 bei 102MHz, wurde angepasst)
	TIM2->ARR = 0xffff;												// 1 Hz
	TIM2->DIER = TIM_DIER_UIE;										// Updateinterrupt an
	NVIC_EnableIRQ(TIM2_IRQn);										// IRQ f�r TIM2 aktivieren
	
	DS1307_Read_Timestamp(&gl_Time);								// Zeitstempel lesen
	gl_Tasktimer_Timestamp = Time_Timestamp_to_UTC(&gl_Time);		// Zeitstempel wandeln
	
	TIM2->CR1 |= TIM_CR1_CEN;										// Counter an
}

uint8_t Tasktimer_Add_Task(uint8_t Position, uint32_t Timestamp ,uint32_t Frequency, void (*FP)(void), const char *Beschreibung)
{
	if((Tasktimer_MAX_Tasks-1) < Position)
	{
		#ifdef Tasktimer_Debug
			USART_Write_String("Tasktimer: Tasktabelle ist voll, Timer kann nicht gesetzt werden\r\n");
		#endif
		return 1;
	}
	
	gl_Tasktimer_Tasks[Position].Frequency = Frequency;
	gl_Tasktimer_Tasks[Position].Timestamp_planned = Timestamp;
	gl_Tasktimer_Functionspointer[Position] = FP;
	gl_Tasktimer_Tasks[Position].Do_task = 0;
	gl_Tasktimer_Tasks[Position].Is_used = 1;
	memcpy(&gl_Tasktimer_Tasks[Position].Text[0],Beschreibung,4);
	return 0;
}

void Tasktimer_Pause_Task(uint8_t Position)
{
	gl_Tasktimer_Tasks[Position].Is_used = 0;
}

void Tasktimer_Restart_Task(uint8_t Position, uint32_t Timestamp, uint32_t Frequency)
{
	gl_Tasktimer_Tasks[Position].Is_used = 1;
	gl_Tasktimer_Tasks[Position].Timestamp_planned = Timestamp;
	gl_Tasktimer_Tasks[Position].Frequency = Frequency;
}

void Tasktimer_Do_Tasks(void)								// Fuehre die Aufgaben aus
{
	for (uint8_t g=0; g<Tasktimer_MAX_Tasks; g++)
	{
		if (gl_Tasktimer_Tasks[g].Do_task)
		{
			gl_Tasktimer_Tasks[g].Do_task = 0;				// Do task Bit auf 0
			gl_Tasktimer_Functionspointer[g]();				// Funktion abarbeiten
		}
	}
}

void Tasktimer_Sync_Clock(void)
{
	gl_Tasktimer_Timestamp = Time_Timestamp_to_UTC(&gl_Time); // Zeit ist maximal eine Sekunde alt
}

void TIM2_IRQHandler(void)
{
	if(TIM2->SR & TIM_SR_UIF)									// Pr�fe ob Interruptbit gesetzt
	{
		TIM2->SR = 0;											// Interruptflag l�schen

		for (uint8_t g=0; g<Tasktimer_MAX_Tasks; g++)			// Zaehle um eins runter und setze ggf. das Bit zum abarbeiten
		{
			if(gl_Tasktimer_Tasks[g].Is_used)
			{
				if (gl_Tasktimer_Tasks[g].Timestamp_planned < gl_Tasktimer_Timestamp)
				{
					gl_Tasktimer_Tasks[g].Do_task = 1;
					gl_Tasktimer_Tasks[g].Timestamp_planned += gl_Tasktimer_Tasks[g].Frequency;	// Neuen Zeitstempel einstellen
				}
			}
		}
		gl_Tasktimer_Timestamp++;								// Z�hle um eine Sekunde hoch
		gl_Tasktimer_Uptime++;									// Z�hle um eine Sekunde hoch
	}
}
