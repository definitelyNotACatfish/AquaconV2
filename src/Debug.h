#ifndef DEBUG_H_
#define DEBUG_H_

// Includes
#include <stm32f4xx.h>

/*

************************************************************************************************
* Command Interface fuer den Webserver ueber den USART auf dem STM32F401RET6				   *
* 2019 � Frederinn															 	   	   *
************************************************************************************************

*/

// Defines
#define Debug_IRQ_Handler		 USART6_IRQHandler										// Welcher Interrupthandler gebraucht wird, IRQ muss von USART aktiviert werden
#define Debug_USART				 USART6													// Welcher USART wird verwendet

// Globale Variablen
extern uint32_t gl_USART_Baud;															// Globale Baud fuer den USART

// Funktionen
extern void Debug_Command_Init(void);													// Init von USART Konsole
extern void Debug_Commands(void);														// Commandofunktion muss in der Main liegen



#endif /* USART_DEBUG_H_ */
