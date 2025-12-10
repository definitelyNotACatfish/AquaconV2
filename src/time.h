#ifndef TIME_H_
#define TIME_H_
// Includes
#include <stdint.h>

/*

************************************************************************************************
* Bibliothek zum Rechnen mit UTC Zeitstempeln												   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
#define	EPOCH_YR					1900														// Epoch beginn, EPOCH = Jan 1 1900 00:00:00
#define	SECS_DAY					86400UL 													//(24L * 60L * 60L)
#define	LEAPYEAR(year)				(!((year) % 4) && (((year) % 100) || !((year) % 400)))		// Checkt ob das Jahr ein Schaltjahr ist
#define	YEARSIZE(year)				(LEAPYEAR(year) ? 366 : 365)								// Gibt die Jahreslaenge in Tagen zurueck, in Abhaengigkeit eines Schaltjahres


// Structs
struct Timestamp
{
	uint8_t Day;
	uint8_t Month;
	uint16_t Year;
	uint8_t Hour;
	uint8_t Minute;
	uint8_t Seconds;
	uint8_t Weekday;
};

// Variablen
extern struct Timestamp gl_Time;															// Globale Zeitvariable

// Funktionen
extern void Time_UTC_to_Timestamp(uint32_t Time, struct Timestamp *_Timestamp);				// UTC in Zeitstempel konvertieren
extern uint8_t Time_Monthlen(uint8_t Is_leapyear,uint8_t Month);							// Gibt die Monatslaenge in Tage zurueck, abhaenging von Schaltjahr
extern uint32_t Time_Timestamp_to_UTC(struct Timestamp *_Timestamp);						// Zeitstempel in UTC Zeit formatieren






#endif


