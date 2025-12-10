// Includes
#include "main.h"
#include "DS2482_100.h"
#include "SoftwareI2C.h"

/*

************************************************************************************************
* DS2482-100 I2C zu One Wire Konverter auf dem STM32 mittels SoftwareI2C					   *
* 14.07.2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen


// Funktionen
uint8_t DS2482_Reset(void)
{
	if(SoftwareI2C_Start(DS2482_Address,I2C_Write)!=0){return 1;}		// Starte Konversation
	SoftwareI2C_Write_Byte(0xf0);										// Reset Befehl
	SoftwareI2C_Stop();													// Stop
	_delay_us(1);														// Wartezeit laut Datenblatt
	return 0;
}

uint8_t DS2482_Set_Read_Pointer(uint8_t Register)
{
	uint8_t Ack=1;
	if(SoftwareI2C_Start(DS2482_Address,I2C_Write)!=0){return 1;}		// Starte Konversation
	SoftwareI2C_Write_Byte(0xe1);										// Set Read Pointer Befehl
	Ack = SoftwareI2C_Write_Byte(Register);								// Schreibe Pointer
	SoftwareI2C_Stop();													// Stop
	return Ack==0 ? 0:1;												// Wenn Ack = 0 dann gebe 0 zur�ck, sonst 1
}

int16_t DS2482_Read_Register(uint8_t Register)
{
	uint8_t ret=0;
	if(DS2482_Set_Read_Pointer(Register)!=0) return -1;					// Setze Lesepointer

	if(SoftwareI2C_Start(DS2482_Address,I2C_Read)!=0){return -1;}		// Starte Konversation
	ret = SoftwareI2C_Read_Nack();										// Lese Byte
	SoftwareI2C_Stop();													// Stop
	return ret;															// Gebe Byte zur�ck
}

uint8_t DS2482_OneWire_Bus_Reset(void)
{
	int16_t reg=0;
	if(SoftwareI2C_Start(DS2482_Address,I2C_Write)!=0){return 1;}		// Starte Konversation
	SoftwareI2C_Write_Byte(0xb4);										// One Wire Reset Befehl
	SoftwareI2C_Stop();													// Stop

	_delay_us(630+613);													// Warte bis Reset fertig

	reg = DS2482_Read_Register(DS2482_Pointer_Status_Register);			// Lese Statusregister
	if(reg==-1) return 1;												// Bei Fehler gebe 1 zur�ck

	return (!((reg & 0x02) >> 1)) & 0x01 ;								// Gebe Precensebit zur�ck
}

uint8_t DS2482_OneWire_Write_Byte(uint8_t Byte)
{
	if(SoftwareI2C_Start(DS2482_Address,I2C_Write)!=0){return 1;}		// Starte Konversation
	SoftwareI2C_Write_Byte(0xa5);										// One Wire 1 Byte schreiben Befehl
	SoftwareI2C_Write_Byte(Byte);										// Byte senden
	SoftwareI2C_Stop();													// Stop
	_delay_us(8*72);													// Warte Zeit ab bis fertig
	return 0;
}

int16_t DS2482_OneWire_Read_Byte(void)
{
	int16_t reg=0;
	if(SoftwareI2C_Start(DS2482_Address,I2C_Write)!=0){return -1;}		// Starte Konversation
	SoftwareI2C_Write_Byte(0x96);										// One Wire 1 Byte schreiben Befehl
	SoftwareI2C_Stop();													// Stop
	_delay_us(8*72);													// Warte Zeit ab bis fertig

	reg = DS2482_Read_Register(DS2482_Pointer_Read_Data_Register);		// Lese Datenregister
	return reg;
}


// Funktionen
