// Includes
#include "main.h"
#include "AT24C32.h"
#include "SoftwareI2C.h"

/*

************************************************************************************************
* AT24C32 EEPROM via Software I2C an STM32F401RET6						  					   *
* 20.05.2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen


// Funktionen
uint8_t AT24C32_Write_Bytes(uint16_t Address, char *Bytes, uint16_t Count)
{
	if (SoftwareI2C_Start(AT24C32_I2C_Address,I2C_Write)==1){return 1;}		// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte((Address>>8) & 0x0f);							// Highbyte Adresse
	SoftwareI2C_Write_Byte(Address & 0xff);									// Lowbyte
	for(uint16_t g=0;g<Count;g++)											// Schreibe Bytes
	{
		SoftwareI2C_Write_Byte(Bytes[g]);
		_delay_us(5000);													// Schreiben eines Bytes dauert 5ms
	}
	SoftwareI2C_Stop();														// Stop
	_delay_us(5000);														// Gebe dem EEPROM noch zeit zum Schreiben
	return 0;
}

uint8_t AT24C32_Read_Bytes(uint16_t Address, char *Bytes, uint16_t Count)
{
	if (SoftwareI2C_Start(AT24C32_I2C_Address,I2C_Write)==1){return 1;}		// Schicke den Start und die Slaveadresse
	SoftwareI2C_Write_Byte((Address>>8) & 0x0f);							// Highbyte Adresse
	SoftwareI2C_Write_Byte(Address & 0xff);									// Lowbyte

	SoftwareI2C_Start(AT24C32_I2C_Address,I2C_Read);						// Schicke den Start und die Slaveadresse
	uint16_t g=0;
	for(;g<(Count-1);g++)													// Lese Bytes
	{
		Bytes[g] = SoftwareI2C_Read_Ack();
	}
	Bytes[g] = SoftwareI2C_Read_Nack();										// Schlie�e das Lesen mit Ack ab

	SoftwareI2C_Stop();														// Stop
	return 0;
}
