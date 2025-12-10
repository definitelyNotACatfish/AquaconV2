// Includes
#include "main.h"
#include "INI_Parser.h"
#include "FAT32.h"
#include <string.h>

/*

************************************************************************************************
* INI-File Format Parser auf dem STM32F407VGT6						   						   *
* 13.08.2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen

// Funktionen
uint8_t INI_Read_Key_String(char *File, char *Section,char *Key, char *Returnstring)
{
	uint8_t returnvalue = 1;															// Ergebniswert, ob der Key gefunden wurde
	char Line[255];
	uint8_t Sectionlength = strlen(&Section[0]);										// Speichere die Stringl�nge der Section ab
	uint8_t Keylength = strlen(&Key[0]);												// Speichere die Stringl�nge des Keys ab
	uint8_t Section_found=0,Key_found=0;												// Merker, dass die Section und Key gefunden wurde

	if(FAT32_File_Open(INI_Parser_Read_Filenumber,&File[0],FAT32_Read)) return 1;		// Falls ein Fehler beim �ffnen der Datei auftritt, breche die Funktion ab
	while(FAT32_File_Read_Line(INI_Parser_Read_Filenumber,&Line[0],sizeof(Line))==0)	// Lese Zeile f�r Zeile aus der Datei aus
	{
		if(Section_found==0 && Key_found==0)											// Wenn noch kein Key und Section gefunden wurde suche erstmal die Section
		{
			if (Line[0]!='[')
			{
				continue;
			}																			// Wenn die Zeile keine Section beinhaltet springe gleich weiter und lese die n�chste Zeile ein
			if(strncmp(&Line[1],&Section[0],Sectionlength)==0) Section_found=1;			// Wenn die Section gefunden wurde, setze den Haken daf�r
			continue;																	// Lese die n�chste Zeile
		}
		if(Section_found==1 && Key_found==0)											// Wenn noch kein Key und Section gefunden wurde suche erstmal die Section
		{
			if (Line[0]=='[') break;													// Wenn die Zeile eine Section beinhaltet, steht der gesuchte Key nicht in der gesuchten Section, beende die while
			if(strncmp(&Line[0],&Key[0],Keylength)==0)									// Wenn der Key gefunden wurde,
			{
				Key_found=1;															// setze den Haken daf�r
				break;																	// und beende die while
			}
		}
	}
	FAT32_File_Close(INI_Parser_Read_Filenumber);
	returnvalue = (Section_found == 1 && Key_found == 1) ? 0 : 1;						// Wenn der Key in der richtigen Section gefunden wurde, gebe 0 zur�ck sonst 1
	if(Section_found == 1 && Key_found == 1)											// Wenn der Key in der richtigen Section gefunden wurde,
	{
		strcpy(&Returnstring[0],strstr(&Line[0],"=")+1);								// Kopiere den Keyvalue in den Returnstring
	}
	else
	{
		*Returnstring=0;																// oder Returnstring ist leer
	}

	return returnvalue;
}

uint8_t INI_Write_Key_String(char *File, char *Section,char *Key, char *Value)
{
	char Line[255],Write_Line[570];
	uint8_t Sectionlength = strlen(&Section[0]);										// Speichere die Stringl�nge der Section ab
	uint8_t Keylength = strlen(&Key[0]);												// Speichere die Stringl�nge des Keys ab
	uint8_t Section_found=0,Key_found=0;												// Merker, dass die Section und Key gefunden wurde

	Section_found=0,Key_found=0;														// Merker, dass die Section und Key gefunden wurde
	if(FAT32_File_Open(INI_Parser_Read_Filenumber,&File[0],FAT32_Read)) return 1;		// Falls ein Fehler beim �ffnen der Datei auftritt, breche die Funktion ab
	FAT32_File_Delete("tmp.ini");														// L�sche tempor�re Datei
	FAT32_File_Create("tmp.ini",0x00);													// Erstelle eine tempor�re Datei, in die die neue INI geschrieben wird
	if(FAT32_File_Open(INI_Parser_Write_Filenumber,"tmp.ini",FAT32_Write)) return 1;	// Falls ein Fehler beim Erstellen der Datei auftritt, breche die Funktion ab
	while(FAT32_File_Read_Line(INI_Parser_Read_Filenumber,&Line[0],sizeof(Line))==0)	// Lese Zeile f�r Zeile aus der Datei aus
	{
		if(Section_found==0 && Key_found==0)											// Wenn noch kein Key und Section gefunden wurde suche erstmal die Section
		{
			strncat(Line,"\r\n",2);
			FAT32_File_Write(INI_Parser_Write_Filenumber,&Line[0],strlen(&Line[0]));	// Schreibe die gelesene Zeile in die tempor�re Datei
			if(strncmp(&Line[1],&Section[0],Sectionlength)==0) Section_found=1;			// Wenn die Section gefunden wurde, setze den Haken daf�r
			continue;																	// Lese die n�chste Zeile
		}
		if(Section_found==1 && Key_found==0)											// Suche den Key in der Section
		{
			if (Line[0]=='[')															// Wenn die n�chste Zeile eine Section ist, dann wurde in der Suchsection der Key nicht gefunden und muss erstellt werden
			{
				strncpy(&Write_Line[0],&Key[0],128);									// Erstelle den Key
				strncat(Write_Line,"=",1);
				strncat(Write_Line,&Value[0],128);
				strncat(Write_Line,"\r\n",2);
				strncat(Line,"\r\n",2);
				strncat(Write_Line,&Line[0],255);										// H�nge die Strings zusmammen um nur einmal in die FAT zu schreiben
				FAT32_File_Write(INI_Parser_Write_Filenumber,&Write_Line[0],strlen(&Write_Line[0])); // Schreibe den neuen Key in die Section
				Key_found=1;															// Key wurde gefunden
				continue;																// Kopiere die n�chsten nachfolgenden Zeilen aus der Datei in die tmp.ini
			}
			if(strncmp(&Line[0],&Key[0],Keylength)==0)									// Wenn der Key gefunden wurde,
			{
				strncpy(&Write_Line[0],&Key[0],128);									// Erstelle den Key
				strncat(Write_Line,"=",1);
				strncat(Write_Line,&Value[0],128);
				strncat(Write_Line,"\r\n",2);
				FAT32_File_Write(INI_Parser_Write_Filenumber,&Write_Line[0],strlen(&Write_Line[0])); // Schreibe den neuen Key in die Section
				Key_found=1;															// Key wurde gefunden
				continue;																// Kopiere die n�chsten nachfolgenden Zeilen aus der Datei in die tmp.ini
			}
			else																		// Kopiere die Keys die nicht gesucht werden in die Section rein
			{
				strncat(Line,"\r\n",2);
				FAT32_File_Write(INI_Parser_Write_Filenumber,&Line[0],strlen(&Line[0]));	// Schreibe die gelesene Zeile in die tempor�re Datei
				continue;
			}
		}
		if(Section_found==1 && Key_found==1)											// Der Key wurde schon in der Section gefunden, trage daher den Rest der Datei in die tmp.ini nach
		{
			strncat(Line,"\r\n",2);
			FAT32_File_Write(INI_Parser_Write_Filenumber,&Line[0],strlen(&Line[0]));	// Schreibe die gelesene Zeile in die tempor�re Datei
			continue;																	// Lese die n�chste Zeile
		}
		// Sollte nie erreicht werden, aber sicher ist sicher
	}
	if(Section_found==1 && Key_found==0)												// Wenn die Section die letze vor Dateiende ist f�ge hier den Key ein
	{
		strncpy(&Write_Line[0],&Key[0],128);											// Erstelle den Key
		strncat(Write_Line,"=",1);
		strncat(Write_Line,&Value[0],128);
		strncat(Write_Line,"\r\n",2);
		FAT32_File_Write(INI_Parser_Write_Filenumber,&Write_Line[0],strlen(&Write_Line[0])); // Schreibe den neuen Key in die Section
	}
	if(Section_found==0 && Key_found==0)												// Wenn noch kein Key und keine Section gefunden wurde, erstelle dessen Eintrag
	{
		Write_Line[0]='[';
		strncpy(&Write_Line[1],&Section[0],128);										// Erstelle die Section
		strncat(Write_Line,"]\r\n",3);
		strncat(&Write_Line[0],&Key[0],128);											// H�nge den Key mit Value dran
		strncat(Write_Line,"=",1);
		strncat(Write_Line,&Value[0],128);
		strncat(Write_Line,"\r\n",2);
		FAT32_File_Write(INI_Parser_Write_Filenumber,&Write_Line[0],strlen(&Write_Line[0])); // Schreibe den neuen Key in die Section
	}
	FAT32_File_Close(INI_Parser_Read_Filenumber);										// Schlie�e die Dateien
	FAT32_File_Close(INI_Parser_Write_Filenumber);
	FAT32_File_Delete(&File[0]);														// L�sche die originale INI
	FAT32_File_Directory_Rename("tmp.ini",&File[0]);									// Benenne die tmp.ini zur originalen Datei um
	return 0;
}

