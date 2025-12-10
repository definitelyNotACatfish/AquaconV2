// Includes
#include "main.h"
#include "DS1307.h"
//#include "delay.h"		// Ist in main.h als Inline
#include "SoftwareI2C.h"
#include <stdio.h>
#include <string.h>

#include "time.h"

/*
******************************************************************
* Routinen zur Ansteuern der RTC DS1307 auf dem STM32F401RE		 *
* 2015-2019 � Frederinn		                             *
******************************************************************

*/	

// Defines


// Globale Variablen
char gl_Timestamp_String[40];
const char gl_Weekday[][11] ={"Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag","Sonntag"};			// Wochentagstrings

uint8_t DS1307_Init(void)
{
	struct Timestamp Timestamp_read;
	DS1307_Write_Controlregister(0,0,0,0);									// Clockausgang aus
 	DS1307_Write_Register(0x00,0b01111111 & DS1307_Read_Register(0x00));	// Maskiere das CH Bit raus. Setze es somit auf 0
 	DS1307_Read_Timestamp(&Timestamp_read);									// und laden
 	_delay_us(1050000);
 	DS1307_Read_Timestamp(&gl_Time);										// und laden
	if (Timestamp_read.Seconds == gl_Time.Seconds)
	{
		return 1;
	}
	return 0;
}

void DS1307_Write_Register(uint8_t Register,uint8_t Value)
{
	_delay_us(5);															// Muss sein
	SoftwareI2C_Start(0b1101000,I2C_Write);									// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte(Register);
	SoftwareI2C_Write_Byte(Value);
	SoftwareI2C_Stop();
}

uint8_t DS1307_Read_Register(uint8_t Register)
{
	uint8_t ReturnValue=0;
	_delay_us(5);															// Muss sein
	SoftwareI2C_Start(0b1101000,I2C_Write);									// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte(Register);
	_delay_us(5);															// Muss sein
	SoftwareI2C_Start(0b1101000,I2C_Read);
	ReturnValue = SoftwareI2C_Read_Nack();
	SoftwareI2C_Stop();
	return ReturnValue;
}

void DS1307_Write_Controlregister(uint8_t OUT, uint8_t SQWE, uint8_t RS1, uint8_t RS0)	// Entweder 0 fuer loeschen oder 1 fuer setzten
{
	uint8_t Register = (OUT<<7)|(SQWE<<4)|(RS1<<1)|(RS0<<0);
	DS1307_Write_Register(0x07,Register);
}

void DS1307_Write_RAM(uint8_t Register,uint8_t Value)						// Rambereich ist zwischen 0x08 und 0x3f
{
	SoftwareI2C_Start(0b1101000,I2C_Write);									// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte(Register);
	SoftwareI2C_Write_Byte(Value);
	SoftwareI2C_Stop();
}

uint8_t DS1307_Read_RAM(uint8_t Register)									// Rambereich ist zwischen 0x08 und 0x3f
{
	uint8_t ReturnValue=0;
	
	SoftwareI2C_Start(0b1101000,I2C_Write);									// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte(Register);
	SoftwareI2C_Start(0b1101000,I2C_Read);
	ReturnValue = SoftwareI2C_Read_Nack();
	SoftwareI2C_Stop();
	
	return ReturnValue;
}

void DS1307_Set_Timestamp(struct Timestamp *Timestamp_new)
{
	uint8_t lSeconds10=0,lSeconds=0,lMinute10=0,lMinute=0,lHour10=0,lHour=0,lDay10=0,lDay=0,lMonthe10=0,lMonthe=0,lYeare10=0,lYeare=0;
	uint8_t cYear=0;
	
	while(Timestamp_new->Day>9)				// Splitte die Daye in 10er und 1er auf
	{
		Timestamp_new->Day-= 10;
		lDay10++;
	}
	lDay = Timestamp_new->Day;				//
	
	lDay |= lDay10 << 4;					// Fuege die Variablen zusammen			
	DS1307_Write_Register(0x04,lDay);
	
	while(Timestamp_new->Month>9)			// Splitte die Monthe in 10er und 1er auf
	{
		Timestamp_new->Month-= 10;
		lMonthe10++;
	}
	lMonthe = Timestamp_new->Month;			//
	
	lMonthe |= lMonthe10 << 4;				// Fuege die Variablen zusammen
	DS1307_Write_Register(0x05,lMonthe);
	
	cYear = (uint8_t)(Timestamp_new->Year - 1980);	// Wandle das Year um
	
	while(cYear>9)							// Splitte die Yeare in 10er und 1er auf
	{
		cYear-= 10;
		lYeare10++;
	}
	lYeare = cYear;							//
	
	lYeare |= lYeare10 << 4;				// Fuege die Variablen zusammen
	DS1307_Write_Register(0x06,lYeare);
	
	while(Timestamp_new->Hour>9)			// Splitte die Monthe in 10er und 1er auf
	{
		Timestamp_new->Hour-= 10;
		lHour10++;
	}
	lHour = Timestamp_new->Hour;			//
	
	lHour |= lHour10 << 4;					// Fuege die Variablen zusammen
	DS1307_Write_Register(0x02,lHour);
	
	while(Timestamp_new->Minute>9)			// Splitte die Monthe in 10er und 1er auf
	{
		Timestamp_new->Minute-= 10;
		lMinute10++;
	}
	lMinute = Timestamp_new->Minute;		//
	
	lMinute |= lMinute10 << 4;				// Fuege die Variablen zusammen
	DS1307_Write_Register(0x01,lMinute);
	
	while(Timestamp_new->Seconds>9)			// Splitte die Monthe in 10er und 1er auf
	{
		Timestamp_new->Seconds-= 10;
		lSeconds10++;
	}
	lSeconds = Timestamp_new->Seconds;		//
	
	lSeconds |= lSeconds10 << 4;			// Fuege die Variablen zusammen
	DS1307_Write_Register(0x00,lSeconds);
	
	DS1307_Write_Register(0x03, Timestamp_new->Weekday+1); // Weekday + 1: Der DS1307 z�hlt von 1-7 wir z�hlen wie normale Menschen von 0-6
}

void DS1307_Read_Timestamp(struct Timestamp *Timestamp_read)										// Daten werden konvertiert in der Struct gespeichert
{
	char Day[11];
	struct Timestamp_raw Time_raw;
 	Time_raw.Seconds = DS1307_Read_Register(0x00);
 	Time_raw.Minute = DS1307_Read_Register(0x01);
 	Time_raw.Hour = DS1307_Read_Register(0x02);
 	Time_raw.Weekday = DS1307_Read_Register(0x03);
 	Time_raw.Day = DS1307_Read_Register(0x04);
 	Time_raw.Month = DS1307_Read_Register(0x05);
	Time_raw.Year = DS1307_Read_Register(0x06);

	Timestamp_read->Seconds = Time_raw.Seconds & 0b00001111;
	Timestamp_read->Seconds += (0b00001111 & (Time_raw.Seconds >> 4))*10;
	
	Timestamp_read->Minute = Time_raw.Minute & 0b00001111;
	Timestamp_read->Minute += (0b00001111 & (Time_raw.Minute >> 4))*10;
	
	Timestamp_read->Hour = Time_raw.Hour & 0b00001111;
	Timestamp_read->Hour += (0b00001111 & (Time_raw.Hour >> 4))*10;
	
	Timestamp_read->Weekday = Time_raw.Weekday-1;													// Weekday - 1: Der DS1307 z�hlt von 1-7 wir z�hlen wie normale Menschen von 0-6
	
	Timestamp_read->Day = Time_raw.Day & 0b00001111;
	Timestamp_read->Day += (0b00001111 & (Time_raw.Day >> 4))*10;
	
	Timestamp_read->Month = Time_raw.Month & 0b00001111;
	Timestamp_read->Month += (0b00001111 & (Time_raw.Month >> 4))*10;
	
	Timestamp_read->Year = Time_raw.Year & 0b00001111;
	Timestamp_read->Year += (0b00001111 & (Time_raw.Year >> 4))*10 + 1980;

	strcpy(&Day[0],gl_Weekday[Timestamp_read->Weekday]);
	sprintf(&gl_Timestamp_String[0],"%s, den %02u.%02u.%04u %02u:%02u:%02u\r\n",&Day[0],Timestamp_read->Day,Timestamp_read->Month,Timestamp_read->Year, Timestamp_read->Hour,Timestamp_read->Minute,Timestamp_read->Seconds);
}
