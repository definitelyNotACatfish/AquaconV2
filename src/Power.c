// Includes
#include "main.h"
#include "ADC.h"
#include "Power.h"
#include "PWM.h"
#include "Geniedisplay.h"
#include <stm32f4xx.h>
#include <stdio.h>
#include "USART.h"

/*

************************************************************************************************
* Leistungserfassung der PWM Ausg�nge der Aquacon auf dem ST32F407VGT6 						   *
* mittels ADC2 und ADC1. TIMER3 k�mmert sich um die Erfassung in die globale R�ckgabe		   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Structs
volatile struct Power gl_Power;

// Variablen

// Funktionen
float Power_Read_Supplyvoltage(void)
{
	uint32_t arr=0;
	float arrf=0.0;
	for(uint32_t g=0;g<2000;g++)													// Mittelwert aus 2000 Messungen �ber 20ms verteilt
	{
		arr+=ADC1_Read_Channel(14,ADC_Samplingrate_480CLK);
		_delay_us(10);
	}
	arrf = arr / 2000.0;

	return arrf * 0.0127344468;														// Unten stehende Formel aufgel�st als Wert
	//return (((double)(3.3/4096.0)*arrf)/1496.0)*23646.0;							// Gebe die Versorgungsspannung zur�ck
}

void Power_Init(void)
{
	// ADC1 und 2 werden in der Init in der main() eingeschaltet
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;												// TIM3 Clock an
	TIM3->CR1 = 0;																	// Upcounter, Edge Aligned
	TIM3->PSC = (F_CPU/65536/10)-1;													// Teiler = PSC + 1
	TIM3->ARR = 65535;																// Ungef�hr 10Hz Updaterate
	TIM3->EGR |= TIM_EGR_UG;														// Preload der Register erzwingen
	TIM3->DIER = TIM_DIER_UIE;														// Updateinterrupt an
	NVIC_SetPriority(TIM3_IRQn,NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 4, 0));// Prio einstellen
	NVIC_EnableIRQ(TIM3_IRQn);														// IRQ f�r TIM3 aktivieren
	TIM3->CR1 |= TIM_CR1_CEN;														// Timer Anschalten
}

void TIM3_IRQHandler(void)
{
	uint16_t Currentarray[6][Power_Current_Counts];									// Speichere X Messwerte pro Kanal
	uint32_t Current_temp=0;

	if(TIM3->SR & TIM_SR_UIF)														// Pr�fe ob Interruptbit gesetzt
	{
		TIM3->SR = 0;																// Interruptflag l�schen
		for(uint8_t count=0;count<Power_Current_Counts;count++)						// Messwerte pro Kanal aufnehmen
		{
			Currentarray[0][count] = ADC2_Read_Channel(10,ADC_Samplingrate_480CLK);
			Currentarray[1][count] = ADC2_Read_Channel(11,ADC_Samplingrate_480CLK);
			Currentarray[2][count] = ADC2_Read_Channel(12,ADC_Samplingrate_480CLK);
			Currentarray[3][count] = ADC2_Read_Channel(13,ADC_Samplingrate_480CLK);
			Currentarray[4][count] = ADC2_Read_Channel(15,ADC_Samplingrate_480CLK);
			Currentarray[5][count] = ADC2_Read_Channel(6,ADC_Samplingrate_480CLK);
			//_delay_us(200);														// PWM ist 1280Hz, der delay reicht um in einer High-Phase den Strom zu messen
		}

		uint8_t valid=0;															// Anzahl g�ltiger Messungen welche gr��er 0.061A sind
		for(uint8_t channel=0;channel<6;channel++)
		{
			valid=0;
			Current_temp=0;
			for(uint8_t count=0;count<Power_Current_Counts;count++)					// G�ltige Messwerte aus Messung ermitteln
			{
				if (Currentarray[channel][count] > 50)								// Wenn der Messwert gr��er 50 Counts, trage diesen zur Mittelwertbildung ein
				{
					Current_temp += Currentarray[channel][count];					// Messewert eintragen
					valid++;
				}
			}
			Current_temp /= (valid>0) ? valid:1;										// Bilde den Mittelwert
			gl_Power.PWM_Channel[channel].Current = Current_temp * 0.001239484;
			//gl_Power.PWM_Channel[channel].Current = ((((double)(3.3/4096.0)*Current_temp)/19.5)/0.0333333);

			gl_Power.PWM_Channel[channel].Current *= (float) gl_PWM_Channel[channel].Duty/PWM_Max; // Wir messen oben den Strom der w�hren des Highzyklus der PWM, um jetzt den Mittelwert �ber einen PWM Zyklus zu erhalten, wird dieser auf den Zyklus gemittelt

		}

		if((gl_Power.PWM_Channel[0].Current > 4.5)|(gl_Power.PWM_Channel[1].Current > 4.5)|(gl_Power.PWM_Channel[2].Current > 4.5)
				|(gl_Power.PWM_Channel[3].Current > 4.5)|(gl_Power.PWM_Channel[4].Current > 4.5)|(gl_Power.PWM_Channel[5].Current > 4.5))
		{
		  PWM_Channel_set(0,0);                     // PWM auf 0
		  PWM_Channel_set(1,0);
		  PWM_Channel_set(2,0);
		  PWM_Channel_set(3,0);
		  PWM_Channel_set(4,0);
		  PWM_Channel_set(5,0);                     //

			PWM_DeInit();															// Wenn ein PWM Ausgang zu viel Strom zieht schalte ab
			Genie_Clear_Display();													// Gebe eine Meldung auf dem Display aus
			Genie_Write_String_5_7(10,28,"PWM �berstrom!");
			while(1);																// und gehe in die Endlosschleife
		}
	}
}

