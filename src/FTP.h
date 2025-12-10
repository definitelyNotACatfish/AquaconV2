#ifndef FTP_H_
#define FTP_H_
// Includes


/*

************************************************************************************************
* FTP Server auf STM32F407VGT6																   *
* 2019-2020 � Frederinn															 	   *
************************************************************************************************

*/

// Defines
//#define FTP_Debug
#define FTP_Command_Port	21														// Komandokanal
#define FTP_Data_Port		4096													// Datenkanal
#define FTP_Max_Entries		5														// Anzahl gleichzeitiger TCP Verbindungen
#define FTP_Filenumber		5														// Welcher Eintrag in gl_FAT32_File[] wird verwendet

// Structs
struct FTP_Table
{
	uint8_t Statemachine_Command;													// Statemachine der Kommandoverbindung
	//uint8_t Statemachine_Data;														// Statemachine der Datenverbindung
	char Workingpath[255];															// Aktueller Arbeitspfad
	char Last_Command[40];															// Letztes Kommando
	uint8_t Client_Command_PiT;														// Client Postition in Table der Commandoverbindung
	uint8_t Client_Data_PiT;														// Client Postition in Table der Datenverbindung
	uint32_t LIST_Filecounter;														// Z�hlt alle Positionen der Dateien im Verzeichnis durch, 0=Kartenname, deswegen wird bei 1 angefangen
	uint8_t LIST_Statemachine;														// Statemachine f�r List
	uint32_t RETR_Dataleft;															// Anzahl der zu sendenden Bytes
	uint8_t STOR_Statemachine;														// Statemachine des STOR Kommandos
};

// Variablen
extern struct FTP_Table gl_FTP_Table;												// FTP Tabelle mit Infos �ber die Verbindungen
extern const char gl_FTP_Months[][4];												// Monate als String
extern char gl_FTP_Buffer[250];														// Sendepuffer

// Funktionen
extern void FTP_Command_Server(uint8_t Position_in_Table);							// Kommandoport Server
extern void FTP_Data_Server(uint8_t Position_in_Table);								// Datenport Server
extern void FTP_List_Send_Entry(uint8_t Position_in_Table);							// Sendet die �bersicht der aktuellen Dir
extern void FTP_RETR_Send_File(uint8_t Position_in_Table);							// Sendet eine Datei an den Client
extern void FTP_STOR_Receive_File(uint8_t Position_in_Table);						// Empf�ngt eine Datei vom Client


#endif



