// Includes
#include "time.h"
#include <stdint.h>
#include "main.h"


/*

************************************************************************************************
* Bibliothek zum Rechnen mit UTC Zeitstempeln												   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Variablen
struct Timestamp gl_Time;																					// Globale Zeitvariable

// Funktionen
void Time_UTC_to_Timestamp(uint32_t Time, struct Timestamp *_Timestamp)										// UTC Convert in Zeitstempel
{
	uint32_t Clockrest;
	uint16_t Number_of_Days;
	
	Number_of_Days = Time / SECS_DAY;																		// Anzahl Tage ermitteln
	Clockrest = Time % SECS_DAY;																			// Rest zur Berechnung der Uhrzeit ermitteln

	_Timestamp->Seconds = Clockrest % 60UL;																	// Sekunden berechnen
	_Timestamp->Minute = (Clockrest % 3600UL) / 60;															// Minuten ausrechnen
	_Timestamp->Hour = Clockrest / 3600UL;																	// Stunden ausrechnen
	_Timestamp->Weekday = Number_of_Days % 7;																// 1.1.1900 war ein Montag
	
	_Timestamp->Year = EPOCH_YR;
	while (Number_of_Days >= YEARSIZE(_Timestamp->Year))													// Z�hle die Jahre solange zu 1900 dazu, bis das jetztige erreicht wurde
	{
		Number_of_Days -= YEARSIZE(_Timestamp->Year);
		_Timestamp->Year++;
	}
	
	_Timestamp->Month=1;
	while (Number_of_Days >= Time_Monthlen(LEAPYEAR(_Timestamp->Year),_Timestamp->Month))					// Selbe noch mit den Monaten
	{
		Number_of_Days -= Time_Monthlen(LEAPYEAR(_Timestamp->Year),_Timestamp->Month);
		_Timestamp->Month++;
	}
	
	_Timestamp->Day = Number_of_Days + 1;																	// Z�hle noch eins dazu, da wir die Tage zum 1.1.1900 dazurechnen
}

uint8_t Time_Monthlen(uint8_t Is_leapyear,uint8_t Month)													// Gibt die Monatslaenge in Tage zurueck, abhaenging von Schaltjahr
{
	if(Month==2)
	{
		return 28 + Is_leapyear;
	}
	else if(Month==1 || Month==3 || Month==5 || Month==7 || Month==8 || Month==10 || Month==12)
	{
		return 31;
	}
	else
	{
		return 30;
	}
}

uint32_t Time_Timestamp_to_UTC(struct Timestamp *_Timestamp)												// Zeitstempel in UTC Zeit formatieren
{
	uint32_t Time=0;
	
	Time += _Timestamp->Seconds;																			// Z�hle die Sekunden
	Time += _Timestamp->Minute * 60;																		// Z�hle die Minuten dazu
	Time += _Timestamp->Hour * 3600UL;																		// Z�hle die Stunden dazu
	Time += (_Timestamp->Day-1) * SECS_DAY;																	// Z�hle die Sekunden im aktuellen Monat dazu
	
	for (uint8_t Month=1; Month<_Timestamp->Month; Month++)													// Z�hle die Sekunden der ganzen Monate hinzu
	{
		Time += Time_Monthlen(LEAPYEAR(_Timestamp->Year),Month) * SECS_DAY;
	}
	
	for (uint16_t Year=EPOCH_YR; Year<_Timestamp->Year; Year++)
	{
		Time += YEARSIZE(Year) * SECS_DAY;
	}
	
	return Time;
}
