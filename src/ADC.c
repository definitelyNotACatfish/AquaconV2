// Includes
#include "main.h"
#include "ADC.h"
#include <stm32f4xx.h>

/*

************************************************************************************************
* ADC Funktionen f�r AquaconV2 auf STM32F407VGT6											   *
* 2019 � Frederinn														 		       *
************************************************************************************************

*/

// Variablen

// Funktionen
void ADC1_Init()
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;										// Port Clock an
	GPIOA->MODER = (GPIOA->MODER & ~( (0b11 << (0*2))|(0b11 << (1*2))|(0b11 << (2*2))|(0b11 << (3*2)) )) | (0b11 << (0*2))|(0b11 << (1*2))|(0b11 << (2*2))|(0b11 << (3*2));		// PA0-PA3 auf Analog Input

	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;										// Port Clock an
	GPIOC->MODER = (GPIOC->MODER & ~( (0b11 << (4*2)) )) | (0b11 << (4*2));		// PC4 auf Analog Input

	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;											// Clock f�r ADC an

	ADC->CCR |= ADC_CCR_ADCPRE_0 | ADC_CCR_ADCPRE_1;							// Clockteiler auf 8
	ADC1->CR2 |= ADC_CR2_EOCS | ADC_CR2_ADON;									// ADC an, Overrundetection an
	_delay_us(3);																// Warte bis der ADC l�uft
}

void ADC2_Init()
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;										// Port Clock an
	GPIOC->MODER = (GPIOC->MODER & ~( (0b11 << (0*2))|(0b11 << (1*2))|(0b11 << (2*2))|(0b11 << (3*2))|(0b11 << (5*2)) )) | (0b11 << (0*2))|(0b11 << (1*2))|(0b11 << (2*2))|(0b11 << (3*2))|(0b11 << (5*2));	// PC0-PC3,PC5 auf Analog Input

	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;										// Port Clock an
	GPIOA->MODER = (GPIOA->MODER & ~( (0b11 << (6*2)) )) | (0b11 << (6*2));		// PA6 auf Analog Input

	RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;											// Clock f�r ADC an

	ADC->CCR |= ADC_CCR_ADCPRE_0 | ADC_CCR_ADCPRE_1;							// Clockteiler auf 8
	ADC2->CR2 |= ADC_CR2_EOCS | ADC_CR2_ADON;									// ADC an, Overrundetection an
	_delay_us(3);																// Warte bis der ADC l�uft
}

uint32_t ADC2_Read_Channel (uint8_t Channel, uint8_t Samplingrate)
{
	ADC2->SQR3 = (Channel & 0x1f);												// An erster Stelle Kanal x
	ADC2->SQR1 |= (1<<20);														// Eine ADC Messung
	if (Channel < 10)															// Samplingrate setzen
	{
		ADC2->SMPR2 = (ADC2->SMPR2 & ~(0b111 << (Channel*3))) | (Samplingrate << (Channel*3));
	}
	else
	{
		ADC2->SMPR1 = (ADC2->SMPR1 & ~(0b111 << ((Channel-10)*3))) | (Samplingrate << ((Channel-10)*3));
	}

	ADC2->CR2 |= ADC_CR2_SWSTART;												// Conversation starten
	while(!((ADC2->SR & ADC_SR_EOC)==ADC_SR_EOC));								// Warte bis fertig

	return ADC2->DR;
}

uint32_t ADC1_Read_Channel (uint8_t Channel, uint8_t Samplingrate)
{	
	ADC1->SQR3 = (Channel & 0x1f);												// An erster Stelle Kanal x
	ADC1->SQR1 |= (1<<20);														// Eine ADC Messung
	if (Channel < 10)															// Samplingrate setzen
	{
		ADC1->SMPR2 = (ADC1->SMPR2 & ~(0b111 << (Channel*3))) | (Samplingrate << (Channel*3));
	}
	else
	{
		ADC1->SMPR1 = (ADC1->SMPR1 & ~(0b111 << ((Channel-10)*3))) | (Samplingrate << ((Channel-10)*3));
	}

	ADC1->CR2 |= ADC_CR2_SWSTART;												// Conversation starten
	while(!((ADC1->SR & ADC_SR_EOC)==ADC_SR_EOC));								// Warte bis fertig

	return ADC1->DR;
}

float ADC1_Read_internal_Temperaturesensor(void)
{
	float Vsensor;
	ADC1->SQR3 = (16<<0);														// An erster Stelle Kanal 16
	ADC1->SQR1 |= (1<<20);														// Eine ADC Messung

	ADC1->SMPR1 |= (0b111<<24);													// Samplingrate VSENS setzen
	ADC1->SMPR1 |= (0b111<<21);													// Samplingrate VREVINT setzen

	ADC->CCR |= ADC_CCR_TSVREFE;												// Tempsensor an
	_delay_us(100);																// Warte 100�s
	ADC1->CR2 |= ADC_CR2_SWSTART;												// Conversation starten
	while(!((ADC1->SR & ADC_SR_EOC)==ADC_SR_EOC));								// Warte bis fertig

	Vsensor = ADC1->DR * (3.3/4096.0);											// Kalkuliere Vsensor
	ADC->CCR &= ~ADC_CCR_TSVREFE;												// Tempsensor aus
	return ((Vsensor - 0.76) / 0.0025) + 25.0;
}
