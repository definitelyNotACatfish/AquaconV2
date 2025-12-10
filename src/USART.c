// Includes
#include "main.h"
#include "USART.h"
#include <stm32f4xx.h>
#include <string.h>
#include <stdio.h>

/*

************************************************************************************************
* USART auf dem STM32F401RET6 						   										   *
* 18.02.2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen
struct USART_buf1 gl_USART;																			// Structure f�r USARTC0

// Funktionen
void __io_putchar(uint8_t Byte)
{
	USART_Write_Byte(Byte);
}

void USART_Init(uint32_t Baud)
{
	RCC->AHB1ENR |= USART_RCC_IOPENR;																																					// Port A Clock an
	USART_PIN_GPIO->MODER = (USART_PIN_GPIO->MODER & ~((0b11 << (USART_RX_PIN*2)) | (0b11 << (USART_TX_PIN*2)))) | (0b10 << (USART_RX_PIN*2))| (0b10 << (USART_TX_PIN*2));				// Maskiere die alte Pinfunktion raus und sete auf alternate function
	USART_AFR |= (USART_RX_AF << (USART_RX_PIN*4)) | (USART_TX_AF << (USART_TX_PIN*4));																					// Alternate function im AFR Register pro Pin eintragen

	RCC->USART_RCC_reg |= USART_RCC_enable_bit;														// Clock f�r USART einschalten
	USART_Regs->BRR = F_CPU / Baud;																	// Baud berechnen
	USART_Regs->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;				// Rx Tx enable, Rx Interrupt, USART einschalten
	NVIC_EnableIRQ(USART_IRQ);																		// IRQ f�r USART aktivieren
}

void USART_Write_Byte(uint8_t Byte)
{
	while(!(USART_Regs->SR & USART_SR_TC));															// Warte bis das Byte gesendet wurde
	USART_Regs->DR = Byte;																			// Sende Byte
}

void USART_Write_String (const char *s)																// Einen String senden
{
    while (*s)
    {
        USART_Write_Byte(*s);
        s++;
    }
}

void USART_Write_X_Bytes(char *Buffer, uint16_t Offset, uint16_t Lenght)
{
	for (uint16_t g = Offset;g < (Lenght+Offset); g++)
	{
			USART_Write_Byte(Buffer[g]);
	}
}

void USART_Clear_RX_Buffer(void)																	// USART Buffer leeren
{
	for(uint16_t g=0;g<USART_Buffer_rx_MAX;g++)
	{
		gl_USART.Buffer_rx[g] = 0x00;																// Hexwert nicht aendern, da in Debug.h diverse Funktionen damit arbeiten
	}
	gl_USART.U0rx = 0;
}
