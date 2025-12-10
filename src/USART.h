#ifndef USART_H_
#define USART_H_

// Includes
#include <stm32f4xx.h>

/*

************************************************************************************************
* USART auf dem STM32F401RET6 						   										   *
* 18.02.2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
// Pinconfig f�r RX und TX
#define USART_RX_PIN			7												// RX Pin an Port
#define USART_TX_PIN			6												// TX Pin an Port
#define	USART_PIN_GPIO			GPIOC											// Welcher GPIO
#define USART_RCC_IOPENR		RCC_AHB1ENR_GPIOCEN								// Welche GPIO Clock an muss
#define USART_AFR				USART_PIN_GPIO->AFR[0]							// AFR Register
#define USART_RX_AF				8												// Welche AFSELy in AFRL f�r RX gilt (Hinweis AFRL geht von Pin 0-7, ARFH von 8-15)
#define USART_TX_AF				8												// Welche AFSELy in AFRL f�r RX gilt (Hinweis AFRL geht von Pin 0-7, ARFH von 8-15)

// Config f�r welchen UART
#define USART_Regs				USART6											// Welcher USART wird eingestellt
#define USART_RCC_reg			APB2ENR											// Clockenable Register f�r den USART
#define USART_RCC_enable_bit	RCC_APB2ENR_USART6EN							// Zu setzendes Bit im Register
#define USART_IRQ				USART6_IRQn										// IRQ Nummer f�r USART

#define USART_Buffer_rx_MAX 	100												// Buffergr��e f�r den Debug RX Buffer in Bytes < 255 Bytes, siehe ISR in USART_Debug

// Structs
struct USART_buf1
{
	char Buffer_rx[USART_Buffer_rx_MAX];										// Buffer fuer USART receive
	unsigned volatile char U0rx;												// Receive counter 0-255
	unsigned volatile char Command_arrived;										// Kommando erhalten
	uint32_t Baud;																// Baud des DebugUSART
};

// Variablen
extern struct USART_buf1 gl_USART;												// Structure f�r USART

// Funktionen
extern void __io_putchar(uint8_t Byte);											// Wird f�r printf gebraucht
extern void USART_Init(uint32_t Baud);											// USART initialisieren
extern void USART_Write_Byte(uint8_t Byte);										// Sende ein Byte
extern void USART_Write_String (const char *s);									// Einen String senden
extern void USART_Write_X_Bytes(char *Buffer, uint16_t Offset, uint16_t Lenght);	// Schreibe X Bytes
extern void USART_Clear_RX_Buffer(void);										// USART Buffer leeren

#endif /* USART_H_ */
