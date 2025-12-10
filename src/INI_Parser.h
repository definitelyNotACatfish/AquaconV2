#ifndef INI_PARSER_H_
#define INI_PARSER_H_
// Includes

/*

************************************************************************************************
* INI-File Format Parser auf dem STM32F407VGT6						   						   *
* 13.08.2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
#define INI_Parser_Read_Filenumber 			11															// Eintragsposition in der FAT32 Datei Tabelle
#define INI_Parser_Write_Filenumber 		12															// Eintragsposition in der FAT32 Datei Tabelle

// Structs

// Variablen

// Funktionen
extern uint8_t INI_Read_Key_String(char *File, char *Section,char *Key, char *Returnstring);			// Liest einen Key von Typ String aus einer INI-Datei aus; return 0=gefunden, 1=nicht gefunden
extern uint8_t INI_Write_Key_String(char *File, char *Section,char *Key, char *Value);					// Schreibt einen Key von Typ String in eine INI-Datei; return 0=geschrieben, 1=nicht geschrieben


#endif /* INI_PARSER_H_ */

// Zum Testen in der Debug.c
/*else if (STRCMP_ALT("INI_R",&gl_USART.Buffer_rx[0])==0)					// INI Section Key
	{
		char Section[128],Key[128];
		char Return[128];

		sscanf(&gl_USART.Buffer_rx[6],"%s %s",&Section[0],&Key[0]);
		gl_FAT32.is_Blocked=1;
		FAT32_Directory_Change("/");
		gl_FAT32.is_Blocked=0;
		INI_Read_Key_String("basic.ini",&Section[0],&Key[0],&Return[0]);
		printf("INI Section:%s Key:%s Value:%s\r\n",&Section[0], &Key[0], &Return[0]);
		return;
	}
	else if (STRCMP_ALT("INI_W",&gl_USART.Buffer_rx[0])==0)					// INI Section Key Value
	{
		char Section[128],Key[128],Value[128];

		sscanf(&gl_USART.Buffer_rx[6],"%s %s %s",&Section[0],&Key[0],&Value[0]);
		gl_FAT32.is_Blocked=1;
		FAT32_Directory_Change("/");
		gl_FAT32.is_Blocked=0;
		INI_Write_Key_String("basic.ini",&Section[0],&Key[0],&Value[0]);
		printf("INI Section:%s Key:%s Value:%s\r\n",&Section[0], &Key[0], &Value[0]);
		return;
	}*/
