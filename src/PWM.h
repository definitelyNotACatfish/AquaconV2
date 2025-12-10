#ifndef PWM_H_
#define PWM_H_

// Includes


/*

************************************************************************************************
* PWM Kan�le f�r die Aquariumsteuerung mit dem STM32F407VGT6 mit TIM1 und TIM8				   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
#define PWM_Resolution		65536
#define PWM_Max				    65535
#define PWM_Min				    0

// Structs
struct PWM_Channel
{
	uint32_t Duty;																			// Duty des Channels
};

// Variablen
extern struct PWM_Channel gl_PWM_Channel[6];												// Globale Variable f�r PWM Channel

// Funktionen
extern void PWM_Init(void);																	// Stellt die PWM Kan�le der Steuerung ein
extern void PWM_DeInit(void);																// Schaltet die PWM Kan�le ab
extern uint8_t PWM_Channel_set(uint8_t Channel, uint32_t Duty);								// Setzt Kanal mit Duty in %; 0=erfolgreich, 1=nok










#endif
