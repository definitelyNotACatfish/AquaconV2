#ifndef SPI_H_
#define SPI_H_

//Includes
#include <stm32f4xx.h>
#include "main.h"

/*

************************************************************************************************
* SPI Kommunikation auf dem STM32F401RET6	 												   *
* 2019 - 2020 � Frederinn													 		   *
************************************************************************************************

*/

//Defines
// Config for Pins
#define SPI_Speed				0b10										// OSPEEDR 00=Low, 01=Medium, 10=High, 11=Very High
#define SPI_MISO_PIN			11											// MISO Pin an Port
#define SPI_MOSI_PIN			12											// MOSI Pin an Port
#define SPI_SCK_PIN				10											// SCK Pin an Port
#define	SPI_PIN_GPIO			GPIOC										// Welcher GPIO
#define SPI_RCC_AHB				AHB1ENR
#define SPI_RCC_IOPENR			RCC_AHB1ENR_GPIOCEN							// Welche GPIO Clock an muss
#define SPI_AFR_Used			1											// Welches AFR wird verwendet 0 oder 1
#define SPI_AFR					SPI_PIN_GPIO->AFR[SPI_AFR_Used]				// SPI_AFR_Used muss noch angepasst werden
#define SPI_MISO_AF				6											// Welche AFSELy in AFRL f�r MISO gilt (Hinweis AFRL geht von Pin 0-7, ARFH von 8-15)
#define SPI_MOSI_AF				6											// Welche AFSELy in AFRL f�r MOSI gilt (Hinweis AFRL geht von Pin 0-7, ARFH von 8-15)
#define SPI_SCK_AF				6											// Welche AFSELy in AFRL f�r SCK gilt (Hinweis AFRL geht von Pin 0-7, ARFH von 8-15)

// Config for SPI
#define SPI_Regs				SPI3										// Welches SPI wird eingestellt
#define SPI_RCC_reg				APB1ENR										// Clockenable Register f�r den SPI
#define SPI_RCC_enable_bit		RCC_APB1ENR_SPI3EN							// Zu setzendes Bit im Register

// Variablen


// Funktionen
extern void SPI_Portinit(void);												// Init der SPI Portpins

// Inlined Funktionen
static inline void SPI_Init(uint8_t Clockdivider) __attribute__ ((always_inline));						// Clockteiler angeben
static inline uint8_t SPI_Read_Write_Byte (uint8_t cData) __attribute__ ((always_inline));				// Ein Byte lesen/schreiben

uint8_t SPI_Read_Write_Byte (uint8_t cData)					// Ein Byte lesen
{
	*(__IO uint8_t *)&SPI_Regs->DR = cData;									// Schreibe Byte
	while(!((SPI_Regs->SR & SPI_SR_TXE) == SPI_SR_TXE));	// Solange Buffer nicht leer, warte
	while(!((SPI_Regs->SR & SPI_SR_RXNE) == SPI_SR_RXNE));	// Warte bis fertig
	while((SPI_Regs->SR & SPI_SR_BSY) == SPI_SR_BSY);		// Warte bis nicht mehr busy
	return *(__IO uint8_t *)&SPI_Regs->DR;									// Gebe das simultan eingelesene Byte zur�ck
}

void SPI_Init(uint8_t Clockdivider)			   				// SPI von Port angeben, Port und Clockteiler angeben
{
	SPI_Regs->CR1 = 0;										// SPI reset, f�r Re-Init
	SPI_Regs->CR1 |= (Clockdivider << 3) | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;	// Clockdiv, SPI Master, CS ist Softwaregesteuert, CS Pin muss high sein, sonst schmei�t das SPI einen MODF Error
	SPI_Regs->CR1 |= SPI_CR1_SPE;							// SPI an
}

#endif /* SPI_H_ */
