#ifndef WEATHER_H_
#define WEATHER_H_
// Includes
#include "time.h"

/*

************************************************************************************************
* Wettersimulation an den 6 PWM-Ausg�ngen der Stuerungssoftware auf dem STM32F401RET6		   *
* TIM5 als 32Bit Ticker																		   *
* 2019 � Frederinn													 		   		   *
************************************************************************************************

*/

// Defines
//#define		Weather_Debug																// Debug Ausgabe aktivieren
#define		Weather_Max_Points		90														// Maximale Anzahl an Punkten in der Tagesplanung, <255
#define		Weather_Filenumber		9														// Dateinummer in der FAT32
#define		Weather_Temp_Filenumber	10														// Dateinummer in der FAT32
#define		Weather_Yesterday		0														// Arrayebene in struct Weather
#define		Weather_Today			1
#define		Weather_Tommorow		2
#define		Weather_Tempcontrol_On	1														// Temperatursteuerung an
#define		Weather_Tempcontrol_Off	0														// Temperatursteuerung aus

//Structs
struct Weather
{
	uint8_t Maintenance;																	// Wartungsbit, 1=true, 0=false; 1=Pausiert die Wettersimulation, 0=l�sst diese wieder weiterlaufen
	uint32_t Systick;																		// Tick in ms, wo sich die Simu gerade befindet
	uint8_t Trigger_Init;																	// Monoflop f�r den Trigger der Init in der Main

	struct chan
	{
		uint8_t Setting;																	// Was ist an dem Kanal angeschlossen; 0=nichts, 1=Growx5, 2=Sunset, 3=Sky, 4=Day, 5=Tropic,6=L�fter, 7=CO2
		uint8_t Manual;																		// 1=Kanal wird von einer anderen Funktion verwendet, 0=Kanal wird von der Tagessimulation mitverwendet
		float Duty;																			// Aktuell errechnete PWM Duty in %
	}Channel[6];
	struct Cool																				// �ber den Webserver kann man eine K�hlfunktion einschalten
	{
		uint8_t used;																		// In Verwendung
		float Treshold;																		// Schwellwert, ab wann die K�hlung einsetzen soll
		uint8_t Fan_Channel;																// Kanal auf dem ein L�fter angeschlossen ist, den die K�hlung steuern kann
	}Cooling;
	struct Weather_Day																		// Checkpunkte aus der jeweiligen Tagesdatei zur Programmsimulation, Punkte f�r gestern, heute, morgen
	{
		float Minutestamp;																	// Minutenstempel
		float PWM_Percent;																	// PWM in Prozent
		uint8_t Channel;																	// Kanal
		uint8_t Point_Used;																	// Punkt wird benutzt, immer 1
	}Days[3][Weather_Max_Points];
};

// Variablen
extern const char gl_Weather_Days[][4];														// Dateinamen der Tage
extern volatile struct Weather gl_Weather;													// Struct f�r die Wettersimulation

// Funktionen
extern void Weather_Tempcontrol(uint8_t On_Off);											 // Temperatur�berwachung an/aus
extern void Weather_Init_Trigger(void);														 // Trigger f�r die Main Schleife
extern void Weather_Init(void);																 // Wetter Init
extern void Weather_Pause(void);															 // Wetter Pausieren
extern void Weather_Sync_Tick(void); 														 // Einmal die Stunde wird die Clock mit der Systemzeit synchronisiert
//void TIM5_IRQHandler(void);																 // Tickt im 100ms Takt
extern void Weather_Set_Channels(uint32_t Systick);											 // Auslagerung des gemeinsamen Funktinonsablaufs, Systick = Systemzeit in ms
extern void Weather_Save_Day(char *Filename, char *Filedata);								 // Speichert einen Tag in den Days Ordner
extern void Weather_Save_Tempcontrol(char *Filename, char *Filedata);						 // Speichere die Temperatursteuerungsdaten ab
extern void Weather_Load_Day(char *Filename, uint8_t Position);								 // L�dt einen Tag aus dem Days Ordner f�r die Simulation in die jeweilige Position (0=gestern, 1=heute, 2=morgen)
extern uint8_t Weather_Find_last_used_Point(uint8_t Channel, uint8_t Day);					 // Suche letzten Punkt im jeweiligen Tag und Kanal (Day 0=gestern, 1=heute, 2=morgen), return < 255 = Position, return = 255 = Fehler
extern uint8_t Weather_Find_first_used_Point(uint8_t Channel, uint8_t Day); 				 // Suche ersten Punkt im jeweiligen Tag und Kanal (Day 0=gestern, 1=heute, 2=morgen), return < 255 = Position, return = 255 = Fehler
extern uint8_t Weather_Find_previous_used_Point(uint8_t Channel, uint8_t Day,uint8_t Point); // Listet den vorherigen Punkt im jeweiligen Tag und Kanal auf; (Day 0=gestern, 1=heute, 2=morgen), return < 255 = Position, return = 255 = Fehler
#endif


