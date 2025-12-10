// Includes
#include "main.h"
#include <stm32f4xx.h>
#include "SPI.h"

/*

************************************************************************************************
* SPI Kommunikation auf dem STM32F401RET6	 												   *
* 2019 - 2020 � Frederinn													 		   *
************************************************************************************************

*/

// Variablen

// Funktionen
void SPI_Portinit(void)
{
	RCC->SPI_RCC_AHB |= SPI_RCC_IOPENR;																																			// Port A Clock an
	SPI_PIN_GPIO->MODER = (SPI_PIN_GPIO->MODER & ~((0b11 << (SPI_MISO_PIN*2)) | (0b11 << (SPI_MOSI_PIN*2))| (0b11 << (SPI_SCK_PIN*2)))) | (0b10 << (SPI_MISO_PIN*2))| (0b10 << (SPI_MOSI_PIN*2))| (0b10 << (SPI_SCK_PIN*2));		// Maskiere die alte Pinfunktion raus und sete auf alternate function
	SPI_PIN_GPIO->OSPEEDR = (SPI_PIN_GPIO->OSPEEDR & ~((0b11 << (SPI_MISO_PIN*2)) | (0b11 << (SPI_MOSI_PIN*2))| (0b11 << (SPI_SCK_PIN*2)))) | (SPI_Speed << (SPI_MISO_PIN*2))| (SPI_Speed << (SPI_MOSI_PIN*2))| (SPI_Speed << (SPI_SCK_PIN*2));	// Geschwindigkeit der Pins einstellen
	SPI_PIN_GPIO->PUPDR |= (SPI_PIN_GPIO->PUPDR & ~(0b11 << (SPI_MISO_PIN*2))) | (0b01 << (SPI_MISO_PIN*2));	// Pullup MISO

#if SPI_AFR_Used == 0
	SPI_AFR |= (SPI_MISO_AF << ((SPI_MISO_PIN)*4)) | (SPI_MOSI_AF << ((SPI_MOSI_PIN)*4))| (SPI_SCK_AF << ((SPI_SCK_PIN)*4));											// Alternate function im AFR Register pro Pin eintragen
#else
	SPI_AFR |= (SPI_MISO_AF << ((SPI_MISO_PIN-8)*4)) | (SPI_MOSI_AF << ((SPI_MOSI_PIN-8)*4))| (SPI_SCK_AF << ((SPI_SCK_PIN-8)*4));											// Alternate function im AFR Register pro Pin eintragen

#endif

	RCC->SPI_RCC_reg |= SPI_RCC_enable_bit;				// Clock f�r SPI einschalten
}


