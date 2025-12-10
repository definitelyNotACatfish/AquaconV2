#ifndef DS2482_100_H_
#define DS2482_100_H_

// Includes


/*

************************************************************************************************
* DS2482-100 I2C zu One Wire Konverter auf dem STM32 mittels SoftwareI2C					   *
* 14.07.2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
#define DS2482_Address 							0b0011000		// 7 Bit Adresse, letzten 2 Bits sind Hardware Adresse
#define DS2482_Pointer_Status_Register			0xf0			// Register Pointer f�r Status
#define DS2482_Pointer_Read_Data_Register		0xe1			// Register Pointer f�r Read Data
#define DS2482_Pointer_Configuration_Register	0xc3			// Register Pointer f�r Config
// Structs


// Variablen


// Funktionen
extern uint8_t DS2482_Reset(void);								// Reset des DS2482; 0=Ok, 1=Fehler
extern int16_t DS2482_Read_Register(uint8_t Register);			// Liest Data von Register; -1=Fehler, >=0 = Data
extern uint8_t DS2482_Set_Read_Pointer(uint8_t Register);		// Setzt den Pointer auf das zu lesende Register; 0=Ok, 1=Fehler
extern uint8_t DS2482_OneWire_Bus_Reset(void);					// One Wire Bus Reset; 1=Fehler, oder nicht erkannt 0=Precense erkannt
extern uint8_t DS2482_OneWire_Write_Byte(uint8_t Byte);			// One Wire Byte Write 1=Fehler, 0=Ok
extern int16_t DS2482_OneWire_Read_Byte(void);					// One Wire Byte Read; -1=Fehler, >=0 = Data

#endif /* DS2482_100_H_ */
