// Includes
#include "main.h"
#include <stm32f4xx.h>
#include "PWM.h"


/*

************************************************************************************************
* PWM Kan�le f�r die Aquariumsteuerung mit dem STM32F407VGT6 mit TIM1 und TIM8				   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines


// Structs


// Variablen
struct PWM_Channel gl_PWM_Channel[6];														// Globale Variable f�r PWM Channel

// Funktionen
void PWM_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;													// Port Clock an
	GPIOA->MODER = (GPIOA->MODER & ~((0b11 << (8*2))|(0b11 << (9*2))|(0b11 << (10*2))|(0b11 << (11*2)))) | (0b10 << (8*2))|(0b10 << (9*2))|(0b10 << (10*2))|(0b10 << (11*2));		// Maskiere die alte Pinfunktion raus und setze auf alternate function
	GPIOA->AFR[1] |= (1 << (3*4))|(1 << (2*4))|(1 << (1*4))|(1 << (0*4));					// Alternate function im AFR Register pro Pin eintragen (PA8-PA11)

	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;													// Port Clock an
	GPIOC->MODER = (GPIOC->MODER & ~((0b11 << (8*2))|(0b11 << (9*2)))) | (0b10 << (8*2))|(0b10 << (9*2));		// Maskiere die alte Pinfunktion raus und setze auf alternate function
	GPIOC->AFR[1] |= (3 << (1*4))|(3 << (0*4));												// Alternate function im AFR Register pro Pin eintragen (PC8, PC9)

	//Starte PWM Timer 1 und 8
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;														// TIM1 Clock an
	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;														// TIM8 Clock an

	TIM1->CR1 = 0;																			// Upcounter, Edge Aligned
	TIM1->PSC = 1;																			// Teiler = PSC + 1
	TIM1->ARR = PWM_Max;																	// 1281Hz
	TIM1->CCMR1 = (0b110 << 12)|(1<<11)|(0b110 << 4)|(1<<3);								// PWM2 Mode, Preload enable, PWM1 Mode, Preload enable
	TIM1->CCMR2 = (0b110 << 12)|(1<<11)|(0b110 << 4)|(1<<3);								// PWM4 Mode, Preload enable, PWM3 Mode, Preload enable
	TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;			// Output f�r PWM an PA8, PC9, PC10, PC11 an
	TIM1->EGR |= TIM_EGR_UG;																// Preload der Register erzwingen
	TIM1->BDTR |= TIM_BDTR_MOE;																// Main Output Enable an

	TIM8->CR1 = 0;																			// Upcounter, Edge Aligned
	TIM8->PSC = 1;																			// Teiler = PSC + 1
	TIM8->ARR = PWM_Max;																	// 1281Hz
	TIM8->CCMR2 = (0b110 << 12)|(1<<11)|(0b110 << 4)|(1<<3);								// PWM4 Mode, Preload enable, PWM3 Mode, Preload enable
	TIM8->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;											// Output f�r PWM an PC8, PC9
	TIM8->EGR |= TIM_EGR_UG;																// Preload der Register erzwingen
	TIM8->BDTR |= TIM_BDTR_MOE;																// Main Output Enable an

	TIM8->CR1 |= TIM_CR1_CEN;																// Counter an
	TIM1->CR1 |= TIM_CR1_CEN;																// Counter an
}

void PWM_DeInit(void)
{
	TIM1->BDTR &= ~TIM_BDTR_MOE;															// Main Output Enable aus
	TIM1->CR1 &= ~TIM_CR1_CEN;																// Counter aus
	TIM1->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E);			// Output f�r PWM an PA8, PC9, PC10, PC11 aus

	TIM8->BDTR &= ~TIM_BDTR_MOE;															// Main Output Enable aus
	TIM8->CR1 &= ~TIM_CR1_CEN;																// Counter an
	TIM8->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC4E);											// Output f�r PWM an PC8, PC9
}

uint8_t PWM_Channel_set(uint8_t Channel, uint32_t Duty)
{	
	if (Channel > 5)																		// Wenn Kanal ung�ltig, breche ab
	{
		return 1;
	}
	
	if ((Duty>PWM_Max)) Duty = PWM_Max;														// Wenn Duty > Max setze auf max
	//if (Duty<PWM_Min) Duty = PWM_Min;														// Selbe f�r min
	
	gl_PWM_Channel[Channel].Duty = Duty;													// Duty anpassen
	
	switch(Channel)																			// Setze den Channel
	{
		case 0:
			TIM1->CCR4 = Duty;
		break;
		
		case 1:
			TIM1->CCR3 = Duty;
		break;
		
		case 2:
			TIM1->CCR2 = Duty;
		break;
		
		case 3:
			TIM1->CCR1 = Duty;
		break;
		
		case 4:
			TIM8->CCR4 = Duty;
		break;
		
		case 5:
			TIM8->CCR3 = Duty;
		break;
		
		default:
			return 1;																		// Wird nie erreicht
		break;
	}
	return 0;																				// Gebe erfolgreich zur�ck
}
