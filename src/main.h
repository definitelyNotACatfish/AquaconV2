#ifndef MAIN_H_
#define MAIN_H_

// Includes
#include <math.h>
#include <stdint.h>

/*

************************************************************************************************
* AquaconV2 auf dem STM32F407VGT6															   *
* 19.02.2019 � Frederinn															   *
************************************************************************************************

*/


// Defines
#define		TempLog_Label_Filenumber	7												// Temperaturlogger Label Filenumber
#define		TempLog_Temp_Filenumber		8												// Temperaturlogger Data Filenumber
#define		F_CPU_Init					102000000UL										// F�r den Initbereich wird diese CPU Frequenz verwendet
#define 	F_CPU 						168000000UL										// CPU Frequenz wird f�r _delay_us ben�tigt
#define 	SPI_Clockdiv_default 		_2												// Default Clockteiler f�r SPI, _4, _8, .. _256
#define 	SPI_Default_Clockdivider 	2UL												// Default Clockteiler als Zahl, 4UL, 8UL, .. 256UL

#define NVIC_PRIORITYGROUP_0         0x00000007U /*!< 0 bits for pre-emption priority
                                                      4 bits for subpriority */
#define NVIC_PRIORITYGROUP_1         0x00000006U /*!< 1 bits for pre-emption priority
                                                      3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         0x00000005U /*!< 2 bits for pre-emption priority
                                                      2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         0x00000004U /*!< 3 bits for pre-emption priority
                                                      1 bits for subpriority */
#define NVIC_PRIORITYGROUP_4         0x00000003U /*!< 4 bits for pre-emption priority
                                                      0 bits for subpriority */

// Clocksels																			// M�ssen in main.h stehen, in SPI.h wird es nicht richtig aufgel�st
#define _2			0
#define _4			1
#define _8			2
#define _16			3
#define _32			4
#define _64     	5
#define _128    	6
#define _256		7


// Structs
struct Webserver
{
	char IP_address[4];																	// IP Adresse des Server
	char Subnetmask[4];																	// Subnetzmaske
	char Gateway_IP[4];																	// Gateway Adresse
	char MAC[6];																		// MAC Adresse
	char NTP_IP[4];																		// NTP IP
};


// Variablen
extern struct Webserver gl_Webserver;													// Globale von Webserver

// Funktionen
extern int main(void);																	// Main Loop
extern void Main_Init_Tasktimer(void);													// Tasktimer einstellen
extern void Main_Read_Timestamp(void);													// Zeitstempel lesen
extern void Main_Genie_refresh(void);													// Display aktualisieren
extern void Main_Temperaturelogger(void);												// Temperaturmessung des DS18B20
extern void Main_Stack_Packetloop_stuff(void);											// Zeug das w�hrend der Stack_Packetloop in der stack.c auch ausgef�hrt werden soll

//Wenn die Funktion direkt nach dem einstellen der CPU Frequenz passiert, kann es durch die Optimierung passieren,
//dass dieser Wartebefehl zu lange dauert. Im weiteren Programmverlauf stimmts dann
static inline void _delay_us(double micros) __attribute__ ((always_inline));			// Delay us
static inline void _delay_us_init(double micros) __attribute__ ((always_inline));		// Delay us f�r den Initbereich

void _delay_us(double micros)
{
	uint32_t wait = (unsigned int)(fabs(micros * ((0.000001/(1.0/F_CPU))/3.0)));			// Die delay_loop sollte 3 Ticks brauchen, deswegen die 3
	if(wait==0){wait=1;}

	asm volatile(
	"ldr r3, %[tick];"
	"1:"
	"subs r3,r3,#1;"
	"bne.n 1b;"
	:
	: [tick] "m" (wait)
	: "r3");
}

void _delay_us_init(double micros)
{
	uint32_t wait = (unsigned int)(fabs(micros * ((0.000001/(1.0/F_CPU_Init))/3.0)));		// Die delay_loop sollte 3 Ticks brauchen, deswegen die 3
	if(wait==0){wait=1;}

	asm volatile(
	"ldr r3, %[tick];"
	"1:"
	"subs r3,r3,#1;"
	"bne.n 1b;"
	:
	: [tick] "m" (wait)
	: "r3");
}



#endif /* MAIN_H_ */
