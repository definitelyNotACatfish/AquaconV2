// Includes
#include "main.h"
#include "DS18B20.h"
#include "DS2482_100.h"
#include <string.h>
#ifdef DS18B20_Debug
	#include <stdio.h>
	#include "USART.h"
#endif

/*

************************************************************************************************
* DS18B20 One Wire Temperatursensor am STM32F401RET6										   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Variablen
struct DS18B20 gl_DS18B20;

// Funktionen
uint8_t DS18B20_Init(struct DS18B20 *Sensor, uint8_t Resolution, float Alarmvalue)
{
	uint8_t result = DS18B20_Read_ROM(Sensor);
	int Alarm = Alarmvalue/0.0625;
	
	if (result != 0)
	{
		#ifdef DS18B20_Debug
			printf("DS18B20: Init nicht moeglich, Errorcode: %u\r\n",result);
		#endif
		return result;
	}
	
	if (DS18B20_Match_ROM(Sensor))
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Init nicht moeglich, kein Device am Bus erkannt\r\n");
		#endif
		return 1;
	}
	
	DS2482_OneWire_Write_Byte(0x4e);			// Write Scratchpad
	DS2482_OneWire_Write_Byte(Alarm & 0xff);
	DS2482_OneWire_Write_Byte((Alarm>>8)&(0xff));
	DS2482_OneWire_Write_Byte(Resolution);
	return 0;
}

uint8_t DS18B20_Read_ROM(struct DS18B20 *Sensor)
{
	uint8_t CRC1=0;
	char Data[8];

	if (DS2482_OneWire_Bus_Reset())
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Read ROM nicht moeglich, kein Device am Bus erkannt\r\n");
		#endif
		return 1;
	}
	DS2482_OneWire_Write_Byte(0x33);		// Nur bei eine Slave
	for (uint8_t g=0;g<8;g++)
	{
		Data[g] = DS2482_OneWire_Read_Byte();
	}
	
	if (Data[0]!=DS18B20_Familytype)
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Read ROM erfolgreich, Device ist aber kein Temperatursensor\r\n");
		#endif
		return 2;
	}
	
	CRC1 = DS18B20_CRC_Calc(&Data[0],7);						// CRC �ber Familycode und Serialnumber

	if (CRC1 != Data[7])
	{
		#ifdef DS18B20_Debug
			printf("DS18B20: Read ROM erfolglos, falsche CRC. Gelesen: 0x%02x, errechnet: 0x%2x\r\n",Data[7],CRC1);
		#endif
		return 3;
	}
	
	memcpy(&Sensor->Serial_Number[0],&Data[1],6);
	#ifdef DS18B20_Debug
		printf("DS18B20: Read ROM erfolgreich. Adresse: 0x%02x%02x%02x%02x%02x%02x\r\n",Sensor->Serial_Number[0],Sensor->Serial_Number[1],Sensor->Serial_Number[2],Sensor->Serial_Number[3],Sensor->Serial_Number[4],Sensor->Serial_Number[5]);
	#endif
	return 0;
}

uint8_t DS18B20_Match_ROM(struct DS18B20 *Sensor)
{
	char Data[8];

	Data[0]=DS18B20_Familytype;
	memcpy(&Data[1],&Sensor->Serial_Number[0],6);
	Data[7] = DS18B20_CRC_Calc(&Data[0],7);
	
	if (DS2482_OneWire_Bus_Reset())
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Match ROM nicht moeglich, kein Device am Bus erkannt\r\n");
		#endif
		return 1;
	}
	DS2482_OneWire_Write_Byte(0x55);
	for (uint8_t g=0;g<8;g++)
	{
		DS2482_OneWire_Write_Byte(Data[g]);
	}
	#ifdef DS18B20_Debug
		USART_Write_String("DS18B20: Match ROM gesendet\r\n");
	#endif
	return 0;
}

uint8_t DS18B20_Start_Temperatureconvert(struct DS18B20 *Sensor, uint8_t Match_ROM)
{
	if (Match_ROM)							// Wenn wahr
	{
		if (DS18B20_Match_ROM(Sensor))		// Spreche bestimmten Sensor an
		{
			#ifdef DS18B20_Debug
				USART_Write_String("DS18B20: Starten der Temperaturwandlung nicht moeglich, kein Device am Bus erkannt\r\n");
			#endif
			return 1;
		}
	}
	else
	{
		if (DS2482_OneWire_Bus_Reset())			// Spreche alle Sensoren an
		{
			#ifdef DS18B20_Debug
				USART_Write_String("DS18B20: Starten der Temperaturwandlung nicht moeglich, kein Device am Bus erkannt\r\n");
			#endif
			return 1;
		}
		DS2482_OneWire_Write_Byte(0xcc);			// Skip ROM
	}
	DS2482_OneWire_Write_Byte(0x44);				// Starte Temperaturwandlung
	return 0;
}

uint8_t DS18B20_Read_Scratchpad(struct DS18B20 *Sensor)
{
	char Data[9];
	uint8_t CRC1=0;
	
	if (DS18B20_Match_ROM(Sensor))
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Scratchpad lesen nicht moeglich, kein Device am Bus erkannt\r\n");
		#endif
		return 1;
	}
	DS2482_OneWire_Write_Byte(0xbe);
	for (uint8_t g=0;g<9;g++)
	{
		Data[g] = DS2482_OneWire_Read_Byte();
	}

	CRC1 = DS18B20_CRC_Calc(&Data[0],8);						// CRC �ber Familycode und Serialnumber

	if (CRC1 != Data[8])
	{
		#ifdef DS18B20_Debug
			printf("DS18B20: Scratchpad lesen erfolglos, falsche CRC. Gelesen: 0x%02x, errechnet: 0x%2x\r\n",Data[7],CRC1);
		#endif
		return 3;
	}
	memcpy(&Sensor->Scratchpad[0],&Data[0],8);
	#ifdef DS18B20_Debug
		USART_Write_String("DS18B20: Scratchpad lesen erfolgreich\r\n");
	#endif
	return 0;
}

float DS18B20_Read_Temperature(struct DS18B20 *Sensor)
{
	if (DS18B20_Read_Scratchpad(Sensor))
	{
		#ifdef DS18B20_Debug
			USART_Write_String("DS18B20: Temperaturlesen nicht moeglich, kein Device am Bus erkannt\r\n");
		#endif
		return -1337.0;
	} 
	#ifdef DS18B20_Debug
		USART_Write_String("DS18B20: Temperaturlesen erfolgreich\r\n");
	#endif
	int16_t tmp = (Sensor->Scratchpad[1]<<8) | Sensor->Scratchpad[0];
	return (float)(tmp) * 0.0625;
}

uint8_t DS18B20_CRC_Calc(char *data, uint8_t length)	 // CRC = X8 + X5 + X4 + 1
{
	uint8_t Input = 0;
	uint8_t Temp = 0;
	uint8_t X0=0, X4=0,X3=0,X8=0;
	
	for(uint8_t i=0;i<length;i++)
	{
		Input = *data++;
		for (uint8_t g=0;g<8;g++)
		{
			X0=X4=X3=X8=0;                                // XOR Merker zur�cksetzen

			X0 = (Input & 0x01) ^ (Temp & 0x01);          // XORe Input und Temp LSB
			X3 = X0 ^ ((Temp & 0b00001000)>>3);           // XORe Bit 3
			X4 = X0 ^ ((Temp & 0b00010000)>>4);           // XORe Bit 4
			X8 = X0;

			Input >>= 1;                                  // Shifte die CRC um eins nach rechts
			Temp >>= 1;                                   // Shifte eins nach rechts
			Temp &= 0b01110011;                           // Maskiere die Bits aus, welche mit dem XOR Ergebnis eingef�gt werden
			Temp |= (X8<<7)|(X4<<3)|(X3<<2);
		}
	}
	return Temp;										  // Gibt die CRC der Daten zur�ck
}
