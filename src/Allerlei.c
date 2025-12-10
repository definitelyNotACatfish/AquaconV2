// Includes
#include "Allerlei.h"
#include <ctype.h>
#include <string.h>
#include <stdint.h>


/*

************************************************************************************************
* Bibliothek mit allm�glichem Schei�														   *
* 2019 - 2020 � Frederinn													 	   	   *
************************************************************************************************

*/

// Globale Variablen
union ctoulong gl_char_to_uint32;																		// Union zum konvertieren von uint32 auf char und umgekehrt

// Funktionen
uint32_t char_to_long_int(char C1,char C2,char C3,char C4)												// MSB First 0x12 0x34 0x56 0x78 -> 0x12345678
{
	return ((uint32_t)(C1)<< 24)|((uint32_t)(C2) << 16)|((uint32_t)(C3) << 8)|((uint32_t)(C4));
}

void char_to_binary(char num, char *Returnstring)														// Char zu Binary Konnvertierung
{
	for (uint8_t i=0; i<8;i++)
	{
		Returnstring[7-i] = 0x30 + ((num >> i) & 1);
	}
	Returnstring[8]=0;
}

void int_to_binary(int num, char *Returnstring)															// int zu Binary Konnvertierung
{
	for (uint8_t i=0; i<16;i++)
	{
		Returnstring[15-i] = 0x30 + ((num >> i) & 1);
	}
	Returnstring[16]=0;
}

void EEPROM_Read_String(char *Dest, uint8_t *EEPROM_Src, uint16_t Len)									// Liest String aus dem EEPROM
{
	uint16_t g=0;
	do
	{
		Dest[g++] = *EEPROM_Src++;
		if ((g+1)==Len) return;
	}while(Dest[g-1]!='\0');																			// Nur Ram ist direkt lesbar :)
}

void EEPROM_Write_String(uint8_t *EEPROM_Dest, char *Src, uint16_t Len)									// Schreibt String ins EEPROM mit maximaler L�nge
{
	while(*Src!='\0')																					// Nur Ram ist direkt lesbar :)
	{
		if (Len==0) return;
		*EEPROM_Dest++ = *Src++;
        Len--;
	}
	EEPROM_Dest = '\0';																					// Nullterminierung
}

void Write_uint32_to_Little_Endian_Buffer(char *Startaddress, uint32_t Value)
{
	Startaddress[3] = (Value>>24) & 0xff;
	Startaddress[2] = (Value>>16) & 0xff;
	Startaddress[1] = (Value>>8) & 0xff;
	Startaddress[0] = Value & 0xff;
}

void Convert_String_to_8_3(char *Returnstring, char *Filename)
{
    uint8_t len = strlen(&Filename[0]);
	memset(&Returnstring[0],' ',11);
	Returnstring[11]=0;
	
	for (uint8_t z=0;z<11;z++)																			// Konvertierung von "Test.txt" zu "TEST    TXT", "test" zu "TEST       "
	{
		if (*Filename == '.')
        { 
			Returnstring[8] = toupper(*++Filename);
            if(len == z+1+1) return;                                                                    // test.t
			Returnstring[9] = toupper(*++Filename);
            if(len == z+1+2) return;                                                                    // test.tt
			Returnstring[10] = toupper(*++Filename);
			return;
		}
		
		if (*Filename == 0 && z < 11) {Returnstring[z] = ' ';}
		else{Returnstring[z] = toupper(*Filename);Filename++;}
	}
}

void Convert_8_3_to_string(char *Returnstring, char *Filename)
{
	uint8_t z=0;
	for (z=0;z<8;z++)																				   // Konvertierung von "TEST    T  " zu "Test.t", "test" zu "TEST       "
	{
		if (Filename[z] != ' '){Returnstring[z] = Filename[z];}
		else{break;}
	}
	if(Filename[8]==' ' && Filename[9]==' ' && Filename[10]==' ')                         			   // Wenn der Dateiname keine Erweiterung hat
	{
		Returnstring[z]=0;
	}
    else                                                                                               // Trage die Dateierweiterung ein
	{
		Returnstring[z]='.';
		Returnstring[z+1]=Filename[8] == ' ' ? 0 : Filename[8];
		Returnstring[z+2]=Filename[9] == ' ' ? 0 : Filename[9];
		Returnstring[z+3]=Filename[10] == ' ' ? 0 : Filename[10];
		Returnstring[z+4]=0;
	}
}

uint64_t char_to_long_long_int(char *Startaddress)													  // MSB First 0x12 0x34 0x56 0x78 0x9A 0xbc 0xde 0xf0-> 0x123456789abcdef0; printf nur als 2x32bit Variable m�glich
{
	return ((uint64_t)(Startaddress[0]&0xff)<<56)|((uint64_t)(Startaddress[1]&0xff)<<48)|((uint64_t)(Startaddress[2]&0xff)<<40)|((uint64_t)(Startaddress[3]&0xff)<<32)|((uint64_t)(Startaddress[4]&0xff)<<24)|((uint64_t)(Startaddress[5]&0xff)<<16)|((uint64_t)(Startaddress[6]&0xff)<<8)|((uint64_t)(Startaddress[7]&0xff));
}

uint8_t Compare_X_Bytes(char *Source, char *Dest, uint32_t len)
{
	for (uint32_t i=0;i<len;i++)
	{
		if (Source[0] != Dest[0])
		{
			return 1;
		}
	}
	return 0;
}

int String_Search_Reverse(char *Array, char Character, uint16_t Arraysize)
{
	for (long int g=(Arraysize-1);g>=0;g--)
	{
		if (Array[g]==Character)
		{
			return g;
		}
	}
	return -1;
}

int String_Search(char *Array, char Character, uint16_t Arraysize)										// Sucht einen Char im String, ansonsten -1
{
	for (uint16_t g=0; g<Arraysize; g++)
	{
		if (Array[g] == Character)
		{
			return g;
		}
	}
	return -1;
}

uint8_t STRCMP_ALT(const char *Command,char *Buffer)													// Prueft ob der String im Buffer mit dem Befehl uebereinstimmt 0=ja, 1=Unbekannter Befehl
{
	while (1)
	{
		if (*Command == 0x00)
		{
			return 0;
		}

		if (*Command == *Buffer)
		{
			Command++;
			Buffer++;
		}
		else
		{
			return 1;
		}
	}
}
