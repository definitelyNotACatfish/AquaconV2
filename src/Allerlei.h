#ifndef ALLERLEI_H_
#define ALLERLEI_H_

// Includes
#include <stdint.h>

/*

************************************************************************************************
* Bibliothek mit allm�glichem Schei�														   *
* 2019 - 2020 � Frederinn													 	   	   *
************************************************************************************************

*/

// Defines


// Structures
union ctoulong
{
	uint32_t Value;
	char arr[4];
};

// Globale Variablen
extern union ctoulong gl_char_to_uint32;													// Union zum konvertieren von uint32 auf char und umgekehrt

// Funktionen
extern uint32_t char_to_long_int(char C1,char C2,char C3,char C4);							// MSB First 0x12 0x34 0x56 0x78 -> 0x12345678
extern uint64_t char_to_long_long_int(char *Startaddress);									// MSB First 0x12 0x34 0x56 0x78 0x9A 0xbc 0xde 0xf0-> 0x123456789abcdef0; printf nur als 2x32bit Variable m�glich
extern void char_to_binary(char num, char *Returnstring);									//Char zu Binary Konnvertierung
extern void int_to_binary(int num, char *Returnstring);										//int zu Binary Konnvertierung
extern void EEPROM_Read_String(char *Dest, uint8_t *EEPROM_Src, uint16_t Len);				// Liest Dateiname mit Maximallaenge im 8.3 Format aus dem EEPROM
extern void EEPROM_Write_String(uint8_t *EEPROM_Dest, char *Src, uint16_t Len);				// Schreibt String ins EEPROM
extern void Write_uint32_to_Little_Endian_Buffer(char *Startaddress, uint32_t Value);		// Schreibe einen uint32 Wert in den Little Endian Buffer
extern void Convert_String_to_8_3(char *Returnstring, char *Filename);						// Wandelt einen Dateinamen oder Ordner in ein 8.3 Format um
extern void Convert_8_3_to_string(char *Returnstring, char *Filename);						// Wandelt ein String im 8.3 Format in einen Normalen Pfad um
extern uint8_t Compare_X_Bytes(char *Source, char *Dest, uint32_t len);						// Alternative zu memcpy
extern int String_Search_Reverse(char *Array, char Character, uint16_t Arraysize);			// Sucht von hinten her einen String nach einem Zeichen ab und gibt die Position aus oder -1
extern int String_Search(char *Array, char Character, uint16_t Arraysize);					// Sucht einen Char im String, ansonsten -1
extern uint8_t STRCMP_ALT(const char *Command,char *Buffer);								// Prueft ob der String im Buffer mit dem Befehl uebereinstimmt 0=ja, 1=Unbekannter Befehl

// Inline Funktionen
static inline uint32_t Get_uint32_from_Little_Endian_Buffer(char *Startaddress) __attribute__ ((always_inline));					// Gibt den umgerechneten long int Wert aus einem Array zurueck das in Little Endian Format ist (z.B. aus SD Karte)


uint32_t Get_uint32_from_Little_Endian_Buffer(char *Startaddress)
{
	return ((uint32_t)(Startaddress[3])<< 24)|((uint32_t)(Startaddress[2]) << 16)|((uint32_t)(Startaddress[1]) << 8)|((uint32_t)(Startaddress[0]));
}



#endif /* ALLERLEI_H_ */
