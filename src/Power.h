#ifndef POWER_H_
#define POWER_H_
// Includes


/*

************************************************************************************************
* Leistungserfassung der PWM Ausg�nge der Aquacon auf dem ST32F407VGT6 						   *
* mittels ADC2 und ADC1. TIMER3 k�mmert sich um die Erfassung in die globale R�ckgabe		   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
#define 	Power_Current_Counts 				128						// Anzahl der Messungen pro PWM Kanal

// Structs
struct Power
{
	struct PWM_Channel1
	{
		float Current;
	}PWM_Channel[6];
};

// Variablen
extern volatile struct Power gl_Power;

// Funktionen
extern float Power_Read_Supplyvoltage(void);							// Lese Versorgungsspannung
extern void Power_Init(void);											// Init der Powerfunktionen
//void TIM3_IRQHandler(void);											// Interrupt zur Strommessung

#endif








