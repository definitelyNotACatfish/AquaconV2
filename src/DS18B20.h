#ifndef DS18B20_H_
#define DS18B20_H_
// Includes

/*

************************************************************************************************
* DS18B20 One Wire Temperatursensor am STM32F401RET6										   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
//#define DS18B20_Debug

#define DS18B20_Familytype			0x28														// Welcher Sensortyp ist der DS18B20
#define DS18B20_Match_ROM_true		1															// ROM aus Serial_Number muss mit Sensor am Bus �bereinstimmen
#define DS18B20_Match_ROM_false		0															// ROM muss nicht �bereinstimmen

#define DS18B20_9Bit_resolution		0x1f														// tCONV = 93.75ms
#define DS18B20_10Bit_resolution	0x3f														// tCONV = 187.5ms
#define DS18B20_11Bit_resolution	0x5f														// tCONV = 375ms
#define DS18B20_12Bit_resolution	0x7f														// tCONV = 750ms

// Structs
struct DS18B20
{
	float Temperature;																			// Gelesene Temperatur des Sensors
	char Serial_Number[6];																		// Serial Number LSB First gespeichert
	char Scratchpad[8];																			// Scratchpad LSB First gespeichert
};

// Variablen
extern struct DS18B20 gl_DS18B20;

// Funktionen
extern uint8_t DS18B20_Init(struct DS18B20 *Sensor, uint8_t Resolution, float Alarmvalue);		// Liest die Serial aus, pr�ft ob es der richtige Familientyp ist, stellt Alarmwert und Aufl�sung ein. Returncodes gleich von DS18B20_Read_ROM
extern uint8_t DS18B20_CRC_Calc(char *data, uint8_t length);									// Berechnet die CRC der Daten und gibt diese zur�ck
extern uint8_t DS18B20_Read_ROM(struct DS18B20 *Sensor);										// Gibt die Adresse eines DS18B20 in die Struct zur�ck, Return 0=ok, 1=kein Device am Bus erkannt, 2=Device kein DS18B20, 3=falsche CRC
extern uint8_t DS18B20_Match_ROM(struct DS18B20 *Sensor);										// Spricht einen Sensor auf dem Bus an, Return 0=ok, 1=Kein Device am Bus erkannt
extern uint8_t DS18B20_Start_Temperatureconvert(struct DS18B20 *Sensor, uint8_t Match_ROM); 	// Startet die Temperaturwandlung an einem (Match_ROM==true), oder allen DS18B20 (false) am Bus, Return 0=ok, 1=Kein Device am Bus
extern uint8_t DS18B20_Read_Scratchpad(struct DS18B20 *Sensor);									// Liest das Scratchpad vom Sensor aus, Return 0=ok, 1=Kein Device am Bus, 3=Falsche CRC
extern float DS18B20_Read_Temperature(struct DS18B20 *Sensor);									// Liest die Temperatur vom Sensor aus und gibt diese Zur�ck. Wenn kein Device am Bus erkannt wurde, Return = -1337.0


#endif


