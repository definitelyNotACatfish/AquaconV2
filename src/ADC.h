#ifndef ADC_H_
#define ADC_H_
// Includes


/*

************************************************************************************************
* ADC Funktionen f�r AquaconV2 auf STM32F407VGT6											   *
* 2019 � Frederinn														 		       *
************************************************************************************************

*/

// Defines
#define ADC_Samplingrate_3CLK 		0														// Samplingrate in Clockzyklen
#define ADC_Samplingrate_15CLK 		1
#define ADC_Samplingrate_28CLK 		2
#define ADC_Samplingrate_56CLK 		3
#define ADC_Samplingrate_84CLK 		4
#define ADC_Samplingrate_112CLK 	5
#define ADC_Samplingrate_144CLK 	6
#define ADC_Samplingrate_480CLK 	7

// Structs


// Variablen


// Funktionen
extern void ADC1_Init();																	// ADC1 Init 12 Bit, regul�r Register
extern void ADC2_Init();																	// ADC2 Init 12 Bit, regul�r Register
extern uint32_t ADC1_Read_Channel (uint8_t Channel, uint8_t Samplingrate);					// Channel = ADC Kanal, Samplingrate
extern uint32_t ADC2_Read_Channel (uint8_t Channel, uint8_t Samplingrate);					// Channel = ADC Kanal, Samplingrate
extern float ADC1_Read_internal_Temperaturesensor(void);									// Internen Tempsensor auslesen










#endif






