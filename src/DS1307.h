#ifndef DS1307_H_
#define DS1307_H_

// Includes
#include "time.h"

/*
******************************************************************
* Routinen zur Ansteuern der RTC DS1307 auf dem STM32F401RE		 *
* 2015-2019 � Frederinn		                             *
******************************************************************

*/	

// Defines
#define	DS1307_Monday		0x01
#define	DS1307_Tuesday		0x02
#define	DS1307_Wednesday	0x03
#define	DS1307_Thursday		0x04
#define	DS1307_Friday		0x05
#define	DS1307_Saturday		0x06
#define	DS1307_Sunday		0x07

// Strukturen
struct Timestamp_raw
{
	uint8_t Day;
	uint8_t Month;
	uint8_t Year;
	uint8_t Hour;
	uint8_t Minute;
	uint8_t Seconds;
	uint8_t Weekday;
};

// Globale Variablen
extern char gl_Timestamp_String[40];
extern const char gl_Weekday[][11];														// Wochentagstrings

// Funktionen
extern uint8_t DS1307_Init(void);
extern void DS1307_Write_Register(uint8_t Register,uint8_t Value);
extern uint8_t DS1307_Read_Register(uint8_t Register);									// gibt bei erfolgreichen lesen den Value zurueck
extern void DS1307_Write_Controlregister(uint8_t ROUT,uint8_t RSQWE,uint8_t RRS1, uint8_t RRS0);
extern void DS1307_Write_RAM(uint8_t Register,uint8_t Value);							// Rambereich ist zwischen 0x08 und 0x3f
extern uint8_t DS1307_Read_RAM(uint8_t Register);										// Rambereich ist zwischen 0x08 und 0x3f
extern void DS1307_Set_Timestamp(struct Timestamp *Timestamp_new);						// Daten werden aus der �bergebenen Struct konvertiert und im DS1307 gespeichert
extern void DS1307_Read_Timestamp(struct Timestamp *Timestamp_read);					// Daten werden konvertiert in der �bergebenen Struct gespeichert




#endif /* DS1307_H_ */
