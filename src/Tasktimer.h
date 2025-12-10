#ifndef TASKTIMER_H_
#define TASKTIMER_H_

// Includes


/*

************************************************************************************************
* Tasktimer mit TIM2 auf dem STM32F401RET6												   	   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
//#define Tasktimer_Debug
#define Tasktimer_MAX_Tasks 15								// Maximale Anzahl an Tasks

// Structs
struct Task
{
	uint8_t Is_used;										// 1 = in Benutzung, 0=nicht
	uint8_t Do_task;										// 1 = Fuehre Aufgabe aus, 0=nicht
	uint32_t Timestamp_planned;								// Zeitstempel wann die Aktion durchgef�hrt werden soll
	uint32_t Frequency;										// Heufigkeit in Sekunden, wann die Aufgabe ausgefuehrt werden muss 1-4294967295
	char Text[5];											// Beschreibung
};


// Variablen
extern struct Task gl_Tasktimer_Tasks[Tasktimer_MAX_Tasks];						// Speicherplatz fuer Tasks reservieren, wegen -fpack-struct die FP Pointer in Variablen umgezogen
extern void (*gl_Tasktimer_Functionspointer[Tasktimer_MAX_Tasks])(void);		// Funktion die Ausgefuehrt werden soll
extern volatile uint32_t gl_Tasktimer_Timestamp;								// Counter f�r die ISR
extern volatile uint32_t gl_Tasktimer_Uptime;									// Uptime des Servers

// Funktionen
extern void Tasktimer_Init(void);																								// Tasktimer Init
extern uint8_t Tasktimer_Add_Task(uint8_t Position, uint32_t Timestamp ,uint32_t Frequency, void (*FP)(void), const char *Beschreibung);	// Task hinzufuegen
extern void Tasktimer_Restart_Task(uint8_t Position, uint32_t Timestamp, uint32_t Frequency);									// Restarte die Aufgabe mit neuer Frequenz
extern void Tasktimer_Pause_Task(uint8_t Position);																				// Pausiere den Task
extern void Tasktimer_Do_Tasks(void);																							// Fuehre die Aufgaben aus
extern void Tasktimer_Sync_Clock(void);																							// Synche den Timestampclock
//void TIM3_IRQHandler(void)




#endif






