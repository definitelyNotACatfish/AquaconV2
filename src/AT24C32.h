#ifndef AT24C32_H_
#define AT24C32_H_
// Includes


/*

************************************************************************************************
* AT24C32 EEPROM via Software I2C an STM32F401RET6						  					   *
* 20.05.2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
#define AT24C32_I2C_Address 0b1010000													// I2C Adresse des EEPROM

// Structs


// Variablen


// Funktionen
extern uint8_t AT24C32_Write_Bytes(uint16_t Address, char *Bytes, uint16_t Count);		// Schreibe X Bytes; Return 1=Fehler, 0=OK
extern uint8_t AT24C32_Read_Bytes(uint16_t Address, char *Bytes, uint16_t Count);		// Lese X Bytes; Return 1=Fehler, 0=OK



#endif /* AT24C32_H_ */
