#ifndef GENIEDISPLAY_H_
#define GENIEDISPLAY_H_

// Includes
#include <stm32f4xx.h>

/*

************************************************************************************************
* Genie Display �ber Software SPI am STM32F401RET6						  					   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
#define Genie_GPIO_VDD																		// Wenn VDD per PIN verwendet wird, dann einkommentieren
#ifdef Genie_GPIO_VDD
	#define Genie_VDD		7																// Pin der die VDD an Display schaltet
#endif
#define Genie_A0			10																// Data = 1, Command = 0
#define Genie_Reset			11																// Reset
#define Genie_CS			12																// Chipselect
#define Genie_SI			8																// SI Genie
#define Genie_SCL			9																// SCL
#define	Genie_PIN_GPIO		GPIOE															// Welcher GPIO
#define Genie_RCC_IOPENR	RCC_AHB1ENR_GPIOEEN												// Welche GPIO Clock an muss
#define Genie_DIR			Genie_PIN_GPIO->MODER											// Direction
#define Genie_OUT			Genie_PIN_GPIO->ODR												// Out
#define Genie_IN			Genie_PIN_GPIO->IDR												// IN

// Structs


// Variablen
extern char gl_Genie_Frame[8][128];															// Framebuffer
extern const char gl_Font_5_7[];															// Font im Flash

// Funktionen
extern void Genie_Init(void);																// Software SPI Master
extern void Genie_Write_Byte(uint8_t Data);													// Send Byte
extern void Genie_Write_Data(uint8_t Data);													// Sende ein Datenbyte
extern void Genie_Write_Command(uint8_t Command);											// Schreibe Command
extern void Genie_Write_Char_5_7(uint8_t x, uint8_t y,uint8_t Character);					// Zeichen Schreiben
extern void Genie_Clear_Display(void);														// Display l�schen
extern void Genie_Write_String_5_7(uint8_t x, uint8_t y, char *String);						// String aus Ram





#endif
